#pragma once

#include <map>

#include "opennfh/core/result.hpp"
#include "opennfh/simulation/actions.hpp"
#include "opennfh/simulation/navigation.hpp"

namespace opennfh::simulation {

struct ControlState {
    EntityId actor{0};
    EntityId pending_target{0};
    Vec2i pending_position;
    std::map<EntityId, ActionTransaction> active_actions;
    Tick last_tick{0};
};

// level_cursor is in level coordinates (room offset already included).
// A zero target means that the click is a walk-to-floor request.
[[nodiscard]] Result<bool> handle_click(
    WorldState& world,
    ControlState& control,
    EntityId actor,
    Vec2i level_cursor,
    EntityId target = 0);

void update_control(WorldState& world, ControlState& control, Tick now);

}  // namespace opennfh::simulation
