#include "opennfh/simulation/actions.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
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

EntityState* find_entity(WorldState& world, EntityId id) {
    for (auto& entity : world.entities) {
        if (entity.id == id && entity.active) {
            return &entity;
        }
    }
    return nullptr;
}

const content::ObjectDef* find_object(const WorldState& world, const std::string& kind) {
    const auto found = world.level.objects.find(kind);
    return found == world.level.objects.end() ? nullptr : &found->second;
}

const content::ActionDef* find_action(const content::ObjectDef& object,
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

int frame_count(const WorldState& world, const content::ObjectDef& object,
                std::string_view name) {
    const content::ObjectDef* current = &object;
    for (int depth = 0; current != nullptr && depth < 8; ++depth) {
        const auto found = current->animations.find(std::string(name));
        if (found != current->animations.end()) {
            return static_cast<int>(found->second.frames.size());
        }
        const auto next = world.level.objects.find(current->gfx);
        if (next == world.level.objects.end() || &next->second == current) break;
        current = &next->second;
    }
    return 0;
}

Result<Tick> duration_for(const WorldState& world, const EntityState& actor,
                          const content::ObjectDef& target, const content::ActionDef& action) {
    if (action.time == "auto") {
        int duration = frame_count(world, target, action.object_animation);
        if (const auto* actor_object = find_object(world, actor.kind); actor_object != nullptr) {
            duration = std::max(duration,
                                frame_count(world, *actor_object, action.actor_animation));
        }
        return Result<Tick>::success(static_cast<Tick>(std::max(duration, 1)));
    }
    Tick value = 0;
    const auto parsed = std::from_chars(action.time.data(), action.time.data() + action.time.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != action.time.data() + action.time.size()) {
        return Result<Tick>::failure(error(ErrorCode::Format, "action time is not an integer or auto"));
    }
    return Result<Tick>::success(value);
}

}  // namespace

Result<ActionTransaction> begin_action(WorldState& world, const ActionRequest& request, Tick now) {
    auto* actor = find_entity(world, request.actor);
    auto* target = find_entity(world, request.target);
    if (actor == nullptr || target == nullptr) {
        return Result<ActionTransaction>::failure(error(ErrorCode::Missing, "action actor or target is missing"));
    }
    if (world.busy_entities.contains(request.actor) || world.busy_entities.contains(request.target)) {
        return Result<ActionTransaction>::failure(error(ErrorCode::InvalidArgument, "action actor or target is busy"));
    }
    const auto* object = find_object(world, target->kind);
    if (object == nullptr) {
        return Result<ActionTransaction>::failure(error(ErrorCode::Missing, "action target definition is missing"));
    }
    const auto* action = find_action(*object, request.action_name, actor->kind);
    if (action == nullptr) {
        return Result<ActionTransaction>::failure(error(ErrorCode::Missing, "action definition is missing"));
    }
    const auto duration = duration_for(world, *actor, *object, *action);
    if (!duration.has_value()) {
        return Result<ActionTransaction>::failure(duration.error());
    }

    world.busy_entities.insert(request.actor);
    world.busy_entities.insert(request.target);
    if (!action->actor_animation.empty()) {
        actor->animation = action->actor_animation;
        actor->animation_started = now;
    }
    if (!action->object_animation.empty()) {
        target->animation = action->object_animation;
        target->animation_started = now;
    }
    ActionTransaction transaction{
        request.actor,
        request.target,
        now,
        duration.value(),
        action->actor_animation,
        action->object_animation,
        action->noise,
        false,
    };
    transaction.actor_next_animation = action->actor_next_animation;
    transaction.object_next_animation = action->object_next_animation;
    return Result<ActionTransaction>::success(std::move(transaction));
}

void advance_action(WorldState& world, ActionTransaction& transaction, Tick now) {
    if (transaction.committed || now < transaction.started || now - transaction.started < transaction.duration) {
        return;
    }
    transaction.committed = true;
    world.busy_entities.erase(transaction.actor);
    world.busy_entities.erase(transaction.target);
    for (auto& entity : world.entities) {
        if (entity.id == transaction.actor && !transaction.actor_next_animation.empty()) {
            entity.animation = transaction.actor_next_animation;
            entity.animation_started = now;
        }
        if (entity.id == transaction.target && !transaction.object_next_animation.empty()) {
            entity.animation = transaction.object_next_animation;
            entity.animation_started = now;
        }
    }
    if (transaction.noise > 0) {
        world.emitted_noise.push_back(NoiseEvent{
            transaction.actor,
            transaction.noise,
            world.current_room(transaction.actor),
            now,
        });
    }
}

}  // namespace opennfh::simulation
