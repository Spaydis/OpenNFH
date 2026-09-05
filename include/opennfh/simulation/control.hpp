#pragma once

#include <map>
#include <cstddef>
#include <optional>
#include <string>

#include "opennfh/core/result.hpp"
#include "opennfh/simulation/actions.hpp"
#include "opennfh/simulation/navigation.hpp"

namespace opennfh::simulation {

enum class DoorTraversalPhase {
    Approach,
    Entering,
    Leaving,
};

struct DoorTraversal {
    EntityId source_door{0};
    EntityId destination_door{0};
    RoomId destination_room;
    Vec2i source_position;
    Vec2i destination_position;
    DoorTraversalPhase phase{DoorTraversalPhase::Approach};
    std::optional<std::size_t> route_index;
};

struct ControlState {
    EntityId actor{0};
    EntityId pending_target{0};
    Vec2i pending_position;
    std::map<EntityId, ActionTransaction> active_actions;
    std::optional<DoorTraversal> door_traversal;
    MovementMode movement_mode{MovementMode::Walk};
    bool movement_active{false};
    std::string idle_animation;
    std::string selected_item;
    std::string pending_action_name;
    Tick last_tick{0};
};

// level_cursor is in level coordinates (room offset already included).
// A zero target means that the click is a walk-to-floor request.
[[nodiscard]] Result<bool> handle_click(
    WorldState& world,
    ControlState& control,
    EntityId actor,
    Vec2i level_cursor,
    EntityId target = 0,
    MovementMode mode = MovementMode::Walk);

void update_control(WorldState& world, ControlState& control, Tick now);

}  // namespace opennfh::simulation
