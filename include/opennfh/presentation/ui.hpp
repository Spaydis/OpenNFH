#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL.h>

#include "opennfh/core/result.hpp"
#include "opennfh/core/types.hpp"
#include "opennfh/io/data_root.hpp"
#include "opennfh/io/xml_fragments.hpp"
#include "opennfh/presentation/viewport.hpp"
#include "opennfh/presentation/renderer.hpp"
#include "opennfh/simulation/replay.hpp"

namespace opennfh::presentation {

using KeyCode = std::uint32_t;
using InputAction = simulation::InputAction;

struct RectI {
    Vec2i offset;
    Vec2i size;
};

struct UiImage {
    std::string name;
    std::string gfx;
};

struct UiControl {
    std::string name;
    RectI rect;
    std::string role;
    std::vector<UiImage> images;
};

struct UiDefinition {
    std::vector<UiControl> controls;
    std::string gfx;
    Vec2i offset;
};

struct UiControlState {
    std::string name;
    RectI rect;
    bool hovered{false};
    bool pressed{false};
    bool enabled{true};
    std::vector<UiImage> images;
};

struct UiBackground {
    std::string asset_id;
    Vec2i position;
};

struct UiSnapshot {
    std::vector<UiControlState> controls;
    std::string background_asset_id;
    Vec2i background_position;
    std::vector<UiBackground> backgrounds;
    std::vector<std::string> inventory_items;
    std::size_t inventory_start{0};
    std::size_t inventory_slots{5};
    std::optional<std::size_t> selected_slot;
};

[[nodiscard]] Result<UiDefinition> parse_dialog(
    const io::XmlFragmentDocument& document,
    std::string_view id = {});

[[nodiscard]] Result<UiDefinition> load_dialog(
    const io::DataRoot& root,
    std::string_view id,
    const io::XmlParseOptions& options = {});

[[nodiscard]] InputAction resolve_shortcut(KeyCode key) noexcept;

void sync_inventory(UiSnapshot& snapshot,
                    const simulation::WorldState& world);

[[nodiscard]] std::optional<std::size_t> hit_ui_button(
    const UiSnapshot& snapshot, Vec2i cursor);

void draw_ui(SDL_Renderer* renderer, const UiSnapshot& snapshot,
             const AssetCache& assets, const ViewportTransform& transform);

void draw_ui(SDL_Renderer* renderer, const UiSnapshot& snapshot,
             const ViewportTransform& transform);

}  // namespace opennfh::presentation
