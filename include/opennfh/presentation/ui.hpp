#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL.h>

#include "opennfh/core/result.hpp"
#include "opennfh/core/types.hpp"
#include "opennfh/io/data_root.hpp"
#include "opennfh/io/xml_fragments.hpp"
#include "opennfh/presentation/viewport.hpp"
#include "opennfh/simulation/replay.hpp"

namespace opennfh::presentation {

using KeyCode = std::uint32_t;
using InputAction = simulation::InputAction;

struct RectI {
    Vec2i offset;
    Vec2i size;
};

struct UiControl {
    std::string name;
    RectI rect;
    std::string role;
};

struct UiDefinition {
    std::vector<UiControl> controls;
};

struct UiControlState {
    std::string name;
    RectI rect;
    bool hovered{false};
    bool pressed{false};
    bool enabled{true};
};

struct UiSnapshot {
    std::vector<UiControlState> controls;
};

[[nodiscard]] Result<UiDefinition> parse_dialog(
    const io::XmlFragmentDocument& document,
    std::string_view id = {});

[[nodiscard]] Result<UiDefinition> load_dialog(
    const io::DataRoot& root,
    std::string_view id,
    const io::XmlParseOptions& options = {});

[[nodiscard]] InputAction resolve_shortcut(KeyCode key) noexcept;

void draw_ui(SDL_Renderer* renderer, const UiSnapshot& snapshot,
             const ViewportTransform& transform);

}  // namespace opennfh::presentation
