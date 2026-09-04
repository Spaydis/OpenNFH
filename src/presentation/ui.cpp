#include "opennfh/presentation/ui.hpp"

#include "opennfh/io/text_codec.hpp"

#include <charconv>
#include <cstddef>
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
        result.controls.push_back(UiControl{std::move(name), rect.value(), std::move(role)});
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

void draw_ui(SDL_Renderer* renderer, const UiSnapshot& snapshot,
             const ViewportTransform& transform) {
    if (renderer == nullptr) {
        return;
    }
    for (const auto& control : snapshot.controls) {
        if (!control.enabled || control.rect.size.x <= 0 || control.rect.size.y <= 0) {
            continue;
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
}

}  // namespace opennfh::presentation
