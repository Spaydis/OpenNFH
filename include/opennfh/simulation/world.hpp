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
};

struct EntityState {
    EntityId id{0};
    std::string kind;
    RoomId room;
    Vec2i position;
    int layer{0};
    bool active{true};
};

struct WorldState {
    content::LevelDefinition level;
    std::vector<EntityState> entities;
    std::set<DoorId> blocked_doors;
    std::map<EntityId, std::vector<NavStep>> pending_paths;
    std::map<EntityId, std::size_t> pending_indices;

    [[nodiscard]] RoomId current_room(EntityId entity) const;
};

}  // namespace opennfh::simulation
