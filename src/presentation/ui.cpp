#include "opennfh/presentation/ui.hpp"

#include "opennfh/io/text_codec.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace opennfh::presentation {

namespace {

Error make_error(ErrorCode code, std::string message, std::string source = {}) {
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.source = std::move(source);
    return error;
}

std::string_view attribute(const io::XmlNode& node, std::string_view name) {
    for (const auto& [key, value] : node.attributes) {
        if (key == name) {
            return value;
        }
    }
    return {};
}

Result<int> parse_integer(std::string_view text, std::string_view source) {
    int value = 0;
    if (text.empty()) {
        return Result<int>::success(0);
    }
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return Result<int>::failure(make_error(ErrorCode::Format, "UI coordinate is not an integer", std::string(source)));
    }
    return Result<int>::success(value);
}

Result<Vec2i> parse_vector(std::string_view text, std::string_view source) {
    if (text.empty()) {
        return Result<Vec2i>::success({});
    }
    const auto separator = text.find('/');
    if (separator == std::string_view::npos) {
        return Result<Vec2i>::failure(make_error(ErrorCode::Format, "UI vector must use x/y syntax", std::string(source)));
    }
    const auto x = parse_integer(text.substr(0, separator), source);
    if (!x.has_value()) {
        return Result<Vec2i>::failure(x.error());
    }
    const auto y = parse_integer(text.substr(separator + 1), source);
    if (!y.has_value()) {
        return Result<Vec2i>::failure(y.error());
    }
    return Result<Vec2i>::success({x.value(), y.value()});
}

Result<RectI> parse_rect(const io::XmlNode& node, std::string_view source) {
    RectI rect;
    const auto offset_text = attribute(node, "offset").empty()
                                 ? attribute(node, "position")
                                 : attribute(node, "offset");
    const auto size_text = attribute(node, "size");
    const auto offset = parse_vector(offset_text, source);
    if (!offset.has_value()) {
        return Result<RectI>::failure(offset.error());
    }
    rect.offset = offset.value();
    if (!size_text.empty()) {
        const auto size = parse_vector(size_text, source);
        if (!size.has_value()) {
            return Result<RectI>::failure(size.error());
        }
        rect.size = size.value();
        return Result<RectI>::success(rect);
    }

    const auto width = parse_integer(attribute(node, "width"), source);
    const auto height = parse_integer(attribute(node, "height"), source);
    if (!width.has_value()) {
        return Result<RectI>::failure(width.error());
    }
    if (!height.has_value()) {
        return Result<RectI>::failure(height.error());
    }
    rect.size = {width.value(), height.value()};
    return Result<RectI>::success(rect);
}

bool is_control(std::string_view name) {
    return name == "button" || name == "text" || name == "label" ||
           name == "image" || name == "progressbar" || name == "slider" ||
           name == "checkbox" || name == "control" || name == "caption";
}

Result<bool> collect_controls(const io::XmlNode& node, UiDefinition& result, std::string_view source) {
    if (is_control(node.name)) {
        auto rect = parse_rect(node, source);
        if (!rect.has_value()) {
            return Result<bool>::failure(rect.error());
        }
        std::string name(attribute(node, "name"));
        if (name.empty()) {
            name = std::string(attribute(node, "id"));
        }
        if (name.empty()) {
            name = node.name;
        }
        std::string role(attribute(node, "role"));
        if (role.empty()) {
            role = node.name;
        }
        std::vector<UiImage> images;
        for (const auto& child : node.children) {
            if (child.name == "image") {
                images.push_back(UiImage{
                    std::string(attribute(child, "name")),
                    std::string(attribute(child, "gfx")),
                });
            }
        }
        result.controls.push_back(UiControl{
            std::move(name), rect.value(), std::move(role), std::move(images)});
        return Result<bool>::success(true);
    }
    for (const auto& child : node.children) {
        const auto collected = collect_controls(child, result, source);
        if (!collected.has_value()) {
            return collected;
        }
    }
    return Result<bool>::success(true);
}

}  // namespace

Result<UiDefinition> parse_dialog(const io::XmlFragmentDocument& document, std::string_view id) {
    const io::XmlNode* dialog = nullptr;
    for (const auto& root : document.roots) {
        if (root.name != "dialog") {
            continue;
        }
        const auto dialog_id = attribute(root, "id").empty() ? attribute(root, "name") : attribute(root, "id");
        if (id.empty() || dialog_id.empty() || dialog_id == id) {
            dialog = &root;
            break;
        }
    }
    if (dialog == nullptr) {
        return Result<UiDefinition>::failure(make_error(ErrorCode::Missing, "dialog is missing", std::string(id)));
    }

    UiDefinition result;
    result.gfx = std::string(attribute(*dialog, "gfx"));
    const auto dialog_offset = parse_vector(
        attribute(*dialog, "offset"), id);
    if (!dialog_offset.has_value()) {
        return Result<UiDefinition>::failure(dialog_offset.error());
    }
    result.offset = dialog_offset.value();
    const auto collected = collect_controls(*dialog, result, std::string(id));
    if (!collected.has_value()) {
        return Result<UiDefinition>::failure(collected.error());
    }
    return Result<UiDefinition>::success(std::move(result));
}

Result<UiDefinition> load_dialog(
    const io::DataRoot& root,
    std::string_view id,
    const io::XmlParseOptions& options) {
    if (id.empty() || id.find("..") != std::string_view::npos ||
        id.front() == '/' || id.front() == '\\') {
        return Result<UiDefinition>::failure(make_error(
            ErrorCode::InvalidArgument, "dialog id is not a safe resource id", std::string(id)));
    }
    const std::string path = "dialogs/" + std::string(id) + ".xml";
    const auto bytes = root.game_data().read(path);
    if (!bytes.has_value()) {
        return Result<UiDefinition>::failure(bytes.error());
    }
    const auto text = io::decode_xml_bytes(bytes.value());
    if (!text.has_value()) {
        return Result<UiDefinition>::failure(text.error());
    }
    const auto document = io::parse_xml_fragments(path, text.value(), options);
    if (!document.has_value()) {
        return Result<UiDefinition>::failure(document.error());
    }
    return parse_dialog(document.value(), id);
}

InputAction resolve_shortcut(KeyCode key) noexcept {
    switch (key) {
    case 0x25: return InputAction::ScrollLeft;
    case 0x26: return InputAction::ScrollUp;
    case 0x27: return InputAction::ScrollRight;
    case 0x28: return InputAction::ScrollDown;
    case 0x13: return InputAction::Pause;
    case 0x1b: return InputAction::Quit;
    default: return InputAction::Unknown;
    }
}

namespace {

bool contains(const RectI& rect, Vec2i point) {
    return rect.size.x > 0 && rect.size.y > 0 &&
           point.x >= rect.offset.x && point.y >= rect.offset.y &&
           point.x < rect.offset.x + rect.size.x &&
           point.y < rect.offset.y + rect.size.y;
}

std::optional<std::size_t> inventory_index(std::string_view name) {
    if (!name.starts_with("inv") || name.size() <= 3) {
        return std::nullopt;
    }
    std::size_t value = 0;
    const auto parsed = std::from_chars(
        name.data() + 3, name.data() + name.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != name.data() + name.size()) {
        return std::nullopt;
    }
    return value;
}

std::string asset_id(std::string_view prefix, std::string_view path,
                     std::string_view state = {}) {
    std::string result(prefix);
    result += path;
    if (!state.empty()) {
        result.push_back(':');
        result += state;
    }
    return result;
}

void draw_asset(SDL_Renderer* renderer, const AssetCache& assets,
                std::string_view id, Vec2i position,
                const ViewportTransform& transform) {
    const auto* image = assets.find(id);
    if (image == nullptr || image->info.width == 0 || image->info.height == 0) {
        return;
    }
    const auto texture = assets.texture(renderer, id);
    if (texture == nullptr) {
        return;
    }
    const auto top_left = transform.to_screen(position);
    const auto bottom_right = transform.to_screen({
        position.x + static_cast<int>(image->info.width),
        position.y + static_cast<int>(image->info.height),
    });
    SDL_FRect destination{
        static_cast<float>(top_left.x),
        static_cast<float>(top_left.y),
        static_cast<float>(bottom_right.x - top_left.x),
        static_cast<float>(bottom_right.y - top_left.y),
    };
    SDL_RenderTexture(renderer, texture, nullptr, &destination);
}

void draw_control_outline(SDL_Renderer* renderer, const UiControlState& control,
                          const ViewportTransform& transform) {
    if (!control.enabled || control.rect.size.x <= 0 || control.rect.size.y <= 0) {
        return;
    }
    const auto top_left = transform.to_screen(control.rect.offset);
    const auto bottom_right = transform.to_screen({
        control.rect.offset.x + control.rect.size.x,
        control.rect.offset.y + control.rect.size.y,
    });
    SDL_FRect rect{
        static_cast<float>(top_left.x),
        static_cast<float>(top_left.y),
        static_cast<float>(bottom_right.x - top_left.x),
        static_cast<float>(bottom_right.y - top_left.y),
    };
    if (control.pressed) {
        SDL_SetRenderDrawColor(renderer, 220, 70, 70, 220);
    } else if (control.hovered) {
        SDL_SetRenderDrawColor(renderer, 240, 200, 80, 220);
    } else {
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 180);
    }
    SDL_RenderRect(renderer, &rect);
}

}  // namespace

void sync_inventory(UiSnapshot& snapshot,
                    const simulation::WorldState& world) {
    const auto slot_count = std::max<std::size_t>(snapshot.inventory_slots, 1);
    const auto maximum_start = world.inventory.size() > slot_count
        ? world.inventory.size() - slot_count : 0;
    snapshot.inventory_start = std::min(snapshot.inventory_start, maximum_start);
    snapshot.inventory_items.clear();
    const auto end = std::min(
        world.inventory.size(), snapshot.inventory_start + slot_count);
    for (std::size_t index = snapshot.inventory_start; index < end; ++index) {
        snapshot.inventory_items.push_back(world.inventory[index]);
    }
    if (snapshot.selected_slot.has_value() &&
        snapshot.selected_slot.value() >= snapshot.inventory_items.size()) {
        snapshot.selected_slot.reset();
    }
}

std::optional<std::size_t> hit_ui_button(
    const UiSnapshot& snapshot, Vec2i cursor) {
    std::optional<std::size_t> result;
    int best_priority = std::numeric_limits<int>::min();
    int best_area = std::numeric_limits<int>::max();
    for (std::size_t index = 0; index < snapshot.controls.size(); ++index) {
        const auto& control = snapshot.controls[index];
        const auto is_button =
            inventory_index(control.name).has_value() ||
            control.name == "left" || control.name == "right" ||
            control.name == "info" || control.name == "center_woody";
        if (!is_button || !control.enabled || !contains(control.rect, cursor)) {
            continue;
        }
        const int priority = inventory_index(control.name).has_value() ? 3 : 2;
        const int area = control.rect.size.x * control.rect.size.y;
        if (!result.has_value() || priority > best_priority ||
            (priority == best_priority && area < best_area)) {
            result = index;
            best_priority = priority;
            best_area = area;
        }
    }
    return result;
}

void draw_ui(SDL_Renderer* renderer, const UiSnapshot& snapshot,
             const AssetCache& assets, const ViewportTransform& transform) {
    if (renderer == nullptr) {
        return;
    }
    for (const auto& background : snapshot.backgrounds) {
        draw_asset(renderer, assets, background.asset_id,
                   background.position, transform);
    }
    if (snapshot.backgrounds.empty() &&
        !snapshot.background_asset_id.empty()) {
        draw_asset(renderer, assets, snapshot.background_asset_id,
                   snapshot.background_position, transform);
    }
    for (const auto& control : snapshot.controls) {
        if (!control.enabled) continue;
        if (control.name == "head_02" || control.name == "head_03" ||
            control.name == "head_04") {
            continue;
        }
        const auto slot = inventory_index(control.name);
        if (slot.has_value()) {
            if (slot.value() < snapshot.inventory_items.size()) {
                const auto& item = snapshot.inventory_items[slot.value()];
                const auto state = snapshot.selected_slot.has_value() &&
                                           snapshot.selected_slot.value() == slot.value()
                                       ? std::string_view{"down"}
                                       : control.hovered
                                           ? std::string_view{"hover"}
                                           : std::string_view{"std"};
                draw_asset(renderer, assets,
                           asset_id("inventory:", item, state),
                           control.rect.offset, transform);
            }
            draw_control_outline(renderer, control, transform);
            continue;
        }

        std::string_view state = control.pressed ? "down"
                              : control.hovered ? "hover" : "std";
        const UiImage* selected = nullptr;
        for (const auto& image : control.images) {
            if (image.name == state) {
                selected = &image;
                break;
            }
            if (selected == nullptr) selected = &image;
        }
        if (selected != nullptr && !selected->gfx.empty()) {
            draw_asset(renderer, assets, asset_id("ui:", selected->gfx),
                       control.rect.offset, transform);
        } else {
            draw_control_outline(renderer, control, transform);
        }
    }
}

void draw_ui(SDL_Renderer* renderer, const UiSnapshot& snapshot,
             const ViewportTransform& transform) {
    if (renderer == nullptr) {
        return;
    }
    for (const auto& control : snapshot.controls) {
        draw_control_outline(renderer, control, transform);
    }
}

}  // namespace opennfh::presentation
