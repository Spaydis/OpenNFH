#include "opennfh/simulation/neighbor_ai.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace opennfh::simulation {

namespace {

bool has_entity_in_room(const WorldState& world, std::string_view kind, std::string_view room) {
    return std::any_of(world.entities.begin(), world.entities.end(), [&](const auto& entity) {
        return entity.active && entity.kind == kind && entity.room == room;
    });
}

bool matches(const WorldState& world, const content::BehaviorDef& behavior,
             const content::TriggerRule& trigger, const NoiseEvent& event) {
    if (trigger.position == "house") {
        return true;
    }
    if (trigger.position == "room") {
        return has_entity_in_room(world, behavior.actor, event.room);
    }
    if (trigger.position == "nearobj") {
        return !trigger.object.empty() && has_entity_in_room(world, trigger.object, event.room);
    }
    return false;
}

std::string trigger_key(const content::BehaviorDef& behavior, const content::TriggerRule& trigger) {
    return behavior.actor + "|" + behavior.name + "|" + trigger.position + "|" + trigger.object;
}

}  // namespace

void dispatch_noise(WorldState& world, const NoiseEvent& event, Tick now) {
    if (event.level <= 0) {
        return;
    }
    for (const auto& behavior : world.level.behaviors) {
        for (const auto& trigger : behavior.triggers) {
            if (!matches(world, behavior, trigger, event)) {
                continue;
            }
            if (trigger.type == "once" && !mark_once_trigger(world, behavior.name, trigger_key(behavior, trigger))) {
                continue;
            }
            if (trigger.type != "once" && trigger.type != "always") {
                continue;
            }
            world.emitted_triggers.push_back(TriggerEvent{
                behavior.actor,
                behavior.name,
                trigger.type,
                trigger.position,
                trigger.object,
                now,
            });
        }
    }
}

void update_neighbor_ai(WorldState& world, Tick now) {
    (void)world;
    (void)now;
}

bool mark_once_trigger(WorldState& world, std::string_view behavior, std::string_view trigger_key_value) {
    const std::string key = std::string(behavior) + "|" + std::string(trigger_key_value);
    return world.fired_triggers.insert(key).second;
}

}  // namespace opennfh::simulation
