#include "opennfh/simulation/control.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "opennfh/simulation/input.hpp"

namespace opennfh::simulation {
namespace {

Error error(ErrorCode code, std::string message) {
    Error result;
    result.code = code;
    result.message = std::move(message);
    return result;
}

EntityState* active_entity(WorldState& world, EntityId id) {
    for (auto& entity : world.entities) {
        if (entity.id == id && entity.active) return &entity;
    }
    return nullptr;
}

const EntityState* active_entity(const WorldState& world, EntityId id) {
    for (const auto& entity : world.entities) {
        if (entity.id == id && entity.active) return &entity;
    }
    return nullptr;
}

const content::Room* room(const WorldState& world, std::string_view id) {
    for (const auto& value : world.level.rooms) {
        if (value.id == id) return &value;
    }
    return nullptr;
}

const content::ObjectDef* object(const WorldState& world, std::string_view kind) {
    const auto found = world.level.objects.find(std::string(kind));
    return found == world.level.objects.end() ? nullptr : &found->second;
}

const EntityState* door_entity(const WorldState& world, std::string_view room_id,
                               std::string_view door_id) {
    for (const auto& entity : world.entities) {
        if (entity.active && entity.room == room_id && entity.kind == door_id) return &entity;
    }
    return nullptr;
}

const content::NeighborLink* link_for_door(const content::Room& room_value,
                                           std::string_view door_id) {
    for (const auto& link : room_value.neighbors) {
        if (link.door_in == door_id) return &link;
    }
    return nullptr;
}

bool has_action(const content::ObjectDef& object_value, std::string_view name,
                std::string_view actor_kind) {
    for (const auto& action : object_value.actions) {
        if (action.name == name && (action.actor.empty() || action.actor == actor_kind)) return true;
    }
    return false;
}

Vec2i add(Vec2i left, Vec2i right) {
    return {left.x + right.x, left.y + right.y};
}

bool inside(const content::Floor& floor, Vec2i point) {
    return point.x >= floor.offset.x && point.y >= floor.offset.y &&
           point.x < floor.offset.x + floor.size.x &&
           point.y < floor.offset.y + floor.size.y;
}

Vec2i project_to_walkway(const content::Room& room_value, Vec2i point) {
    if (!room_value.floors.empty() &&
        std::none_of(room_value.floors.begin(), room_value.floors.end(),
                     [&](const auto& floor) { return inside(floor, point); })) {
        const auto& floor = room_value.floors.front();
        point.x = std::clamp(point.x, floor.offset.x, floor.offset.x + std::max(floor.size.x - 1, 0));
        point.y = std::clamp(point.y, floor.offset.y, floor.offset.y + std::max(floor.size.y - 1, 0));
    }
    const auto path_x = room_value.path2.x - room_value.path1.x;
    const auto path_y = room_value.path2.y - room_value.path1.y;
    const auto length_squared = path_x * path_x + path_y * path_y;
    if (length_squared > 0) {
        const auto point_x = point.x - room_value.path1.x;
        const auto point_y = point.y - room_value.path1.y;
        const auto projected = static_cast<double>(point_x * path_x + point_y * path_y) /
                               static_cast<double>(length_squared);
        const auto clamped = std::clamp(projected, 0.0, 1.0);
        point.x = room_value.path1.x + static_cast<int>(std::lround(clamped * path_x));
        point.y = room_value.path1.y + static_cast<int>(std::lround(clamped * path_y));
    }
    return point;
}

const content::Room* room_at_point(const WorldState& world, Vec2i point) {
    const content::Room* path_match = nullptr;
    for (const auto& value : world.level.rooms) {
        const Vec2i local{
            point.x - value.offset.x,
            point.y - value.offset.y,
        };
        if (std::any_of(value.floors.begin(), value.floors.end(),
                        [&](const auto& floor) { return inside(floor, local); })) {
            return &value;
        }
        const auto min_x = std::min(value.path1.x, value.path2.x) - 48;
        const auto max_x = std::max(value.path1.x, value.path2.x) + 48;
        const auto min_y = std::min(value.path1.y, value.path2.y) - 48;
        const auto max_y = std::max(value.path1.y, value.path2.y) + 48;
        if (local.x >= min_x && local.x <= max_x &&
            local.y >= min_y && local.y <= max_y) {
            path_match = &value;
        }
    }
    return path_match;
}

Vec2i interaction_position(const WorldState& world, const EntityState& target,
                           std::string_view actor_kind) {
    Vec2i result = target.position;
    const auto* target_object = object(world, target.kind);
    if (target_object == nullptr) return result;
    for (const auto& hotspot : target_object->hotspots) {
        if (hotspot.name == actor_kind || hotspot.name.empty()) {
            return add(result, hotspot.offset);
        }
    }
    if (!target_object->hotspots.empty()) return add(result, target_object->hotspots.front().offset);
    return result;
}

Result<ActionRequest> request_for_control(
    const WorldState& world, EntityId actor, EntityId target,
    std::string_view explicit_action) {
    const auto* target_state = active_entity(world, target);
    if (target_state == nullptr) {
        return Result<ActionRequest>::failure(error(ErrorCode::Missing, "control target is not active"));
    }
    if (!explicit_action.empty()) {
        return action_request_for(world, actor, target, explicit_action);
    }
    // Many original objects expose open as their standard action and close as
    // a secondary action. Choose close when the object is already open.
    if (target_state->animation == "open") {
        const auto close = action_request_for(world, actor, target, "close");
        if (close.has_value()) return close;
    }
    return action_request_for(world, actor, target);
}

Result<bool> queue_door_traversal(WorldState& world, ControlState& control,
                                  EntityId actor, const EntityState& door_state,
                                  const EntityState& actor_state) {
    const auto* current_room = room(world, actor_state.room);
    if (current_room == nullptr) {
        return Result<bool>::failure(error(ErrorCode::Missing, "door source room is missing"));
    }
    const auto* link = link_for_door(*current_room, door_state.kind);
    if (link == nullptr || link->door_out.empty()) {
        return Result<bool>::failure(error(ErrorCode::Missing, "door has no room transition"));
    }
    if (world.blocked_doors.contains(link->door_in) ||
        world.blocked_doors.contains(link->door_out)) {
        return Result<bool>::failure(error(ErrorCode::InvalidArgument, "door transition is blocked"));
    }
    const auto* destination_room = room(world, link->name);
    const auto* destination_door = door_entity(world, link->name, link->door_out);
    const auto* source_object = object(world, door_state.kind);
    const auto* destination_object = destination_door == nullptr
        ? nullptr : object(world, destination_door->kind);
    if (destination_room == nullptr || destination_door == nullptr ||
        source_object == nullptr || destination_object == nullptr) {
        return Result<bool>::failure(error(ErrorCode::Missing, "door destination is incomplete"));
    }
    if (!has_action(*source_object, "enter", actor_state.kind) ||
        !has_action(*destination_object, "leave", actor_state.kind)) {
        return Result<bool>::failure(error(ErrorCode::Missing, "door enter/leave binding is missing"));
    }
    const auto source_position = interaction_position(world, door_state, actor_state.kind);
    const auto destination_position = interaction_position(
        world, *destination_door, actor_state.kind);
    const auto queued = walk_to(world, actor, actor_state.room, source_position);
    if (!queued.has_value()) return queued;
    control.door_traversal = DoorTraversal{
        door_state.id, destination_door->id, destination_room->id,
        source_position, destination_position, DoorTraversalPhase::Approach};
    return Result<bool>::success(true);
}

bool start_route_door_traversal(
    WorldState& world, ControlState& control, Tick now) {
    if (control.door_traversal.has_value() ||
        world.busy_entities.contains(control.actor)) {
        return false;
    }
    const auto path = world.pending_paths.find(control.actor);
    const auto index = world.pending_indices.find(control.actor);
    auto* actor_state = active_entity(world, control.actor);
    if (path == world.pending_paths.end() ||
        index == world.pending_indices.end() || actor_state == nullptr) {
        return false;
    }
    std::size_t route_index = index->second;
    if (route_index + 1 >= path->second.size() ||
        path->second[route_index].door.empty() ||
        path->second[route_index].destination_door.empty() ||
        path->second[route_index + 1].room ==
            path->second[route_index].room) {
        if (route_index == 0) {
            return false;
        }
        --route_index;
        if (route_index + 1 >= path->second.size()) {
            return false;
        }
    }
    const auto& departure = path->second[route_index];
    const auto& arrival = path->second[route_index + 1];
    if (departure.door.empty() || arrival.room == departure.room) {
        return false;
    }
    if (departure.destination_door.empty()) {
        actor_state->room = arrival.room;
        actor_state->position = arrival.destination;
        index->second = route_index + 2;
        return true;
    }
    const auto* source_door = door_entity(world, departure.room, departure.door);
    const auto* destination_door = door_entity(
        world, arrival.room, departure.destination_door);
    if (actor_state->room != departure.room ||
        actor_state->position.x != departure.destination.x ||
        actor_state->position.y != departure.destination.y) {
        return false;
    }
    if (source_door == nullptr || destination_door == nullptr) {
        actor_state->room = arrival.room;
        actor_state->position = arrival.destination;
        index->second = route_index + 2;
        return true;
    }
    const auto* source_object = object(world, source_door->kind);
    const auto* destination_object = object(world, destination_door->kind);
    if (source_object == nullptr || destination_object == nullptr ||
        !has_action(*source_object, "enter", actor_state->kind) ||
        !has_action(*destination_object, "leave", actor_state->kind)) {
        actor_state->room = arrival.room;
        actor_state->position = arrival.destination;
        index->second = route_index + 2;
        return true;
    }
    const auto started = begin_action(world, {
        control.actor, source_door->id, "enter"}, now);
    if (!started.has_value()) {
        actor_state->room = arrival.room;
        actor_state->position = arrival.destination;
        index->second = route_index + 2;
        return false;
    }
    control.active_actions[started.value().actor] = started.value();
    control.door_traversal = DoorTraversal{
        source_door->id,
        destination_door->id,
        arrival.room,
        departure.destination,
        arrival.destination,
        DoorTraversalPhase::Entering,
        route_index,
    };
    control.movement_active = false;
    return true;
}

}  // namespace

Result<bool> handle_click(
    WorldState& world,
    ControlState& control,
    EntityId actor,
    Vec2i level_cursor,
    EntityId target,
    MovementMode mode) {
    auto* actor_state = active_entity(world, actor);
    if (actor_state == nullptr) {
        return Result<bool>::failure(error(ErrorCode::Missing, "controlled actor is not active"));
    }
    if (world.busy_entities.contains(actor)) {
        return Result<bool>::failure(error(ErrorCode::InvalidArgument, "controlled actor is busy"));
    }
    control.actor = actor;
    control.movement_mode = mode;
    if (!control.movement_active) {
        control.idle_animation = actor_state->animation;
    }
    control.movement_active = true;
    control.pending_target = 0;
    control.pending_position = {};
    control.pending_action_name.clear();

    if (target != 0) {
        const auto* target_state = active_entity(world, target);
        if (target_state == nullptr || target == actor) {
            return Result<bool>::failure(error(ErrorCode::Missing, "clicked target is not active"));
        }
        const auto* target_object = object(world, target_state->kind);
        if (target_object != nullptr && target_object->kind == "door") {
            return queue_door_traversal(world, control, actor, *target_state, *actor_state);
        }
        const auto destination = interaction_position(world, *target_state, actor_state->kind);
        const auto queued = walk_to(world, actor, target_state->room, destination);
        if (!queued.has_value()) return queued;
        control.pending_target = target;
        control.pending_position = destination;
        control.pending_action_name = control.selected_item;
        return Result<bool>::success(true);
    }

    const auto* current = room(world, actor_state->room);
    if (current == nullptr) {
        return Result<bool>::failure(error(ErrorCode::Missing, "controlled actor room is missing"));
    }
    const auto* clicked_room = room_at_point(world, level_cursor);
    const auto* destination_room =
        clicked_room == nullptr ? current : clicked_room;
    const Vec2i local{
        level_cursor.x - destination_room->offset.x,
        level_cursor.y - destination_room->offset.y,
    };
    const auto destination = project_to_walkway(*destination_room, local);
    return walk_to(world, actor, destination_room->id, destination);
}

void update_control(WorldState& world, ControlState& control, Tick now) {
    control.last_tick = now;
    if (control.actor == 0) return;

    start_route_door_traversal(world, control, now);
    advance_walking(world, control.actor, control.movement_mode, 0, now);
    if (control.pending_target != 0 &&
        world.pending_paths.find(control.actor) == world.pending_paths.end()) {
        const auto* actor_state = active_entity(world, control.actor);
        const auto* target_state = active_entity(world, control.pending_target);
        if (actor_state != nullptr && target_state != nullptr &&
            actor_state->room == target_state->room &&
            actor_state->position.x == control.pending_position.x &&
            actor_state->position.y == control.pending_position.y) {
            const auto request = request_for_control(
                world, control.actor, control.pending_target,
                control.pending_action_name);
            if (request.has_value()) {
                const auto started = begin_action(world, request.value(), now);
                if (started.has_value()) {
                    control.active_actions[started.value().actor] = started.value();
                    control.movement_active = false;
                }
            }
        }
        control.pending_target = 0;
    }

    if (control.door_traversal.has_value() &&
        control.door_traversal->phase == DoorTraversalPhase::Approach &&
        world.pending_paths.find(control.actor) == world.pending_paths.end()) {
        const auto* actor_state = active_entity(world, control.actor);
        const auto* source_door = active_entity(world, control.door_traversal->source_door);
        if (actor_state != nullptr && source_door != nullptr &&
            actor_state->room == source_door->room &&
            actor_state->position.x == control.door_traversal->source_position.x &&
            actor_state->position.y == control.door_traversal->source_position.y) {
            const auto started = begin_action(world, {
                control.actor, control.door_traversal->source_door, "enter"}, now);
            if (started.has_value()) {
                control.active_actions[started.value().actor] = started.value();
                control.door_traversal->phase = DoorTraversalPhase::Entering;
                control.movement_active = false;
            } else {
                control.door_traversal.reset();
            }
        }
    }

    for (auto action = control.active_actions.begin(); action != control.active_actions.end();) {
        advance_action(world, action->second, now);
        if (action->second.committed) {
            const auto completed = action->second;
            action = control.active_actions.erase(action);
            if (control.door_traversal.has_value() &&
                completed.actor == control.actor &&
                control.door_traversal->phase == DoorTraversalPhase::Entering &&
                completed.target == control.door_traversal->source_door) {
                if (auto* actor_state = active_entity(world, control.actor); actor_state != nullptr) {
                    actor_state->room = control.door_traversal->destination_room;
                    actor_state->position = control.door_traversal->destination_position;
                    const auto started = begin_action(world, {
                        control.actor, control.door_traversal->destination_door, "leave"}, now);
                    if (started.has_value()) {
                        control.active_actions[started.value().actor] = started.value();
                        control.door_traversal->phase = DoorTraversalPhase::Leaving;
                    } else {
                        control.door_traversal.reset();
                    }
                } else {
                    control.door_traversal.reset();
                }
            } else if (control.door_traversal.has_value() &&
                       control.door_traversal->phase == DoorTraversalPhase::Leaving &&
                       completed.target == control.door_traversal->destination_door) {
                if (control.door_traversal->route_index.has_value()) {
                    const auto index = world.pending_indices.find(control.actor);
                    if (index != world.pending_indices.end() &&
                        (index->second == control.door_traversal->route_index.value() ||
                         index->second == control.door_traversal->route_index.value() + 1)) {
                        index->second =
                            control.door_traversal->route_index.value() + 2;
                    }
                }
                control.door_traversal.reset();
            }
        } else {
            ++action;
        }
    }

    if (control.movement_active &&
        world.pending_paths.find(control.actor) == world.pending_paths.end() &&
        !control.door_traversal.has_value() &&
        control.active_actions.find(control.actor) == control.active_actions.end()) {
        if (auto* actor_state = active_entity(world, control.actor);
            actor_state != nullptr && !control.idle_animation.empty()) {
            actor_state->animation = control.idle_animation;
            actor_state->animation_started = now;
        }
        control.movement_active = false;
    }
}

}  // namespace opennfh::simulation
