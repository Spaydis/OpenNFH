#include "opennfh/simulation/input.hpp"

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace opennfh::simulation {

namespace {

Error error(ErrorCode code, std::string message) {
    Error result;
    result.code = code;
    result.message = std::move(message);
    return result;
}

EntityState* find_active(WorldState& world, EntityId id) {
    for (auto& entity : world.entities) {
        if (entity.id == id && entity.active) {
            return &entity;
        }
    }
    return nullptr;
}

const EntityState* find_active(const WorldState& world, EntityId id) {
    for (const auto& entity : world.entities) {
        if (entity.id == id && entity.active) {
            return &entity;
        }
    }
    return nullptr;
}

const content::ObjectDef* find_object(
    const WorldState& world,
    std::string_view kind) {
    const auto found = world.level.objects.find(std::string(kind));
    return found == world.level.objects.end() ? nullptr : &found->second;
}

const content::ActionDef* find_action(
    const content::ObjectDef& object,
    std::string_view name,
    std::string_view actor_kind) {
    const content::ActionDef* fallback = nullptr;
    for (const auto& action : object.actions) {
        if (action.name != name) continue;
        if (action.actor == actor_kind) return &action;
        if (action.actor.empty()) fallback = &action;
    }
    return fallback;
}

bool contains(const HitRegion& region, Vec2i cursor) {
    if (!region.active || region.size.x <= 0 || region.size.y <= 0) {
        return false;
    }
    const auto right = static_cast<std::int64_t>(region.offset.x) + region.size.x;
    const auto bottom = static_cast<std::int64_t>(region.offset.y) + region.size.y;
    return static_cast<std::int64_t>(cursor.x) >= region.offset.x &&
           static_cast<std::int64_t>(cursor.x) < right &&
           static_cast<std::int64_t>(cursor.y) >= region.offset.y &&
           static_cast<std::int64_t>(cursor.y) < bottom;
}

}  // namespace

Result<EntityId> hit_test(
    std::span<const HitRegion> regions,
    Vec2i cursor) {
    const HitRegion* best = nullptr;
    for (const auto& region : regions) {
        if (!contains(region, cursor)) {
            continue;
        }
        if (best == nullptr ||
            region.layer > best->layer ||
            (region.layer == best->layer && region.y_order > best->y_order) ||
            (region.layer == best->layer && region.y_order == best->y_order &&
             region.source_order > best->source_order)) {
            best = &region;
        }
    }
    if (best == nullptr) {
        return Result<EntityId>::failure(error(
            ErrorCode::Missing, "no active hit region contains the cursor"));
    }
    return Result<EntityId>::success(best->entity);
}

Result<EntityId> resolve_target(
    const WorldState& world,
    std::string_view target) {
    if (target.empty()) {
        return Result<EntityId>::failure(error(
            ErrorCode::InvalidArgument, "input target is empty"));
    }
    if (target.starts_with("entity:")) {
        const auto number = target.substr(7);
        EntityId id = 0;
        const auto parsed = std::from_chars(number.data(), number.data() + number.size(), id);
        if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size()) {
            return Result<EntityId>::failure(error(
                ErrorCode::Format, "entity target ID is malformed"));
        }
        if (find_active(world, id) == nullptr) {
            return Result<EntityId>::failure(error(
                ErrorCode::Missing, "entity target is not active"));
        }
        return Result<EntityId>::success(id);
    }
    for (const auto& entity : world.entities) {
        if (entity.active && entity.kind == target) {
            return Result<EntityId>::success(entity.id);
        }
    }
    return Result<EntityId>::failure(error(
        ErrorCode::Missing, "named input target is not active"));
}

Result<ActionRequest> action_request_for(
    const WorldState& world,
    EntityId actor,
    EntityId target,
    std::string_view explicit_action) {
    const auto* actor_entity = find_active(world, actor);
    const auto* target_entity = find_active(world, target);
    if (actor_entity == nullptr || target_entity == nullptr) {
        return Result<ActionRequest>::failure(error(
            ErrorCode::Missing, "action actor or target is not active"));
    }
    if (actor_entity->room != target_entity->room) {
        return Result<ActionRequest>::failure(error(
            ErrorCode::InvalidArgument, "action actor and target are in different rooms"));
    }
    const auto* object = find_object(world, target_entity->kind);
    if (object == nullptr) {
        return Result<ActionRequest>::failure(error(
            ErrorCode::Missing, "action target definition is missing"));
    }

    const content::ActionDef* selected = nullptr;
    if (!explicit_action.empty()) {
        selected = find_action(*object, explicit_action, actor_entity->kind);
    } else {
        for (const auto& name : object->standard_actions) {
            selected = find_action(*object, name, actor_entity->kind);
            if (selected != nullptr) {
                break;
            }
        }
    }
    if (selected == nullptr) {
        return Result<ActionRequest>::failure(error(
            ErrorCode::Missing, "action binding is missing"));
    }
    if (!selected->actor.empty() && selected->actor != actor_entity->kind) {
        return Result<ActionRequest>::failure(error(
            ErrorCode::InvalidArgument, "action actor kind does not match"));
    }
    return Result<ActionRequest>::success(ActionRequest{
        actor, target, selected->name});
}

}  // namespace opennfh::simulation
