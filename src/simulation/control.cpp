#include "opennfh/simulation/control.hpp"

#include <algorithm>
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
    if (room_value.path1.x != room_value.path2.x ||
        room_value.path1.y != room_value.path2.y) {
        if (room_value.path1.x == room_value.path2.x) {
            point.x = room_value.path1.x;
        } else if (room_value.path1.y == room_value.path2.y) {
            point.y = room_value.path1.y;
        } else {
            const auto min_x = std::min(room_value.path1.x, room_value.path2.x);
            const auto max_x = std::max(room_value.path1.x, room_value.path2.x);
            const auto min_y = std::min(room_value.path1.y, room_value.path2.y);
            const auto max_y = std::max(room_value.path1.y, room_value.path2.y);
            point.x = std::clamp(point.x, min_x, max_x);
            point.y = std::clamp(point.y, min_y, max_y);
        }
    }
    return point;
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
    const WorldState& world, EntityId actor, EntityId target) {
    const auto* target_state = active_entity(world, target);
    if (target_state == nullptr) {
        return Result<ActionRequest>::failure(error(ErrorCode::Missing, "control target is not active"));
    }
    // Many original objects expose open as their standard action and close as
    // a secondary action. Choose close when the object is already open.
    if (target_state->animation == "open") {
        const auto close = action_request_for(world, actor, target, "close");
        if (close.has_value()) return close;
    }
    return action_request_for(world, actor, target);
}

}  // namespace

Result<bool> handle_click(
    WorldState& world,
    ControlState& control,
    EntityId actor,
    Vec2i level_cursor,
    EntityId target) {
    auto* actor_state = active_entity(world, actor);
    if (actor_state == nullptr) {
        return Result<bool>::failure(error(ErrorCode::Missing, "controlled actor is not active"));
    }
    if (world.busy_entities.contains(actor)) {
        return Result<bool>::failure(error(ErrorCode::InvalidArgument, "controlled actor is busy"));
    }
    control.actor = actor;
    control.pending_target = 0;
    control.pending_position = {};

    if (target != 0) {
        const auto* target_state = active_entity(world, target);
        if (target_state == nullptr || target == actor) {
            return Result<bool>::failure(error(ErrorCode::Missing, "clicked target is not active"));
        }
        const auto destination = interaction_position(world, *target_state, actor_state->kind);
        const auto queued = walk_to(world, actor, target_state->room, destination);
        if (!queued.has_value()) return queued;
        control.pending_target = target;
        control.pending_position = destination;
        return Result<bool>::success(true);
    }

    const auto* current = room(world, actor_state->room);
    if (current == nullptr) {
        return Result<bool>::failure(error(ErrorCode::Missing, "controlled actor room is missing"));
    }
    if (world.level.meta.size.x > 0 && world.level.meta.size.y > 0 &&
        (level_cursor.x < 0 || level_cursor.y < 0 ||
         level_cursor.x >= world.level.meta.size.x ||
         level_cursor.y >= world.level.meta.size.y)) {
        return Result<bool>::failure(error(ErrorCode::InvalidArgument, "floor click is outside the level"));
    }
    const Vec2i local{
        level_cursor.x - current->offset.x,
        level_cursor.y - current->offset.y,
    };
    const auto destination = project_to_walkway(*current, local);
    return walk_to(world, actor, current->id, destination);
}

void update_control(WorldState& world, ControlState& control, Tick now) {
    control.last_tick = now;
    if (control.actor == 0) return;

    advance_walking(world, control.actor);
    if (control.pending_target != 0 &&
        world.pending_paths.find(control.actor) == world.pending_paths.end()) {
        const auto* actor_state = active_entity(world, control.actor);
        const auto* target_state = active_entity(world, control.pending_target);
        if (actor_state != nullptr && target_state != nullptr &&
            actor_state->room == target_state->room &&
            actor_state->position.x == control.pending_position.x &&
            actor_state->position.y == control.pending_position.y) {
            const auto request = request_for_control(world, control.actor, control.pending_target);
            if (request.has_value()) {
                const auto started = begin_action(world, request.value(), now);
                if (started.has_value()) {
                    control.active_actions[started.value().actor] = started.value();
                }
            }
        }
        control.pending_target = 0;
    }

    for (auto action = control.active_actions.begin(); action != control.active_actions.end();) {
        advance_action(world, action->second, now);
        if (action->second.committed) {
            action = control.active_actions.erase(action);
        } else {
            ++action;
        }
    }
}

}  // namespace opennfh::simulation
