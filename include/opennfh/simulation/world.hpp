#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "opennfh/content/model.hpp"

namespace opennfh::simulation {

using EntityId = std::uint32_t;
using Tick = std::uint64_t;
using RoomId = content::RoomId;
using DoorId = content::DoorId;

struct NavStep {
    RoomId room;
    DoorId door;
    Vec2i destination;
    int cost{0};
    Vec2i arrival;
    DoorId destination_door;
};

struct EntityState {
    EntityId id{0};
    std::string kind;
    RoomId room;
    Vec2i position;
    int layer{0};
    bool active{true};
    bool visible{true};
    std::string animation;
    Tick animation_started{0};
};

struct NoiseEvent {
    EntityId source{0};
    int level{0};
    RoomId room;
    Tick tick{0};
};

struct TriggerEvent {
    std::string actor;
    std::string behavior;
    std::string type;
    std::string position;
    std::string object;
    Tick tick{0};
};

struct WorldState {
    content::LevelDefinition level;
    std::vector<EntityState> entities;
    std::set<DoorId> blocked_doors;
    std::map<EntityId, std::vector<NavStep>> pending_paths;
    std::map<EntityId, std::size_t> pending_indices;
    std::map<EntityId, std::map<std::string, int>> object_contents;
    std::map<std::string, bool> flags;
    std::map<std::string, int> contents;
    std::vector<std::string> inventory;
    std::map<std::string, int> quotas;
    std::set<EntityId> busy_entities;
    std::vector<NoiseEvent> emitted_noise;
    std::set<std::string> fired_triggers;
    std::vector<TriggerEvent> emitted_triggers;

    [[nodiscard]] RoomId current_room(EntityId entity) const;
};

}  // namespace opennfh::simulation
