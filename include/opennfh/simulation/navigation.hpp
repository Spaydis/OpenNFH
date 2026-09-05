#pragma once

#include <string_view>
#include <vector>

#include "opennfh/core/result.hpp"

#include "opennfh/simulation/world.hpp"

namespace opennfh::simulation {

[[nodiscard]] Result<std::vector<NavStep>> find_path(
    const WorldState& world,
    EntityId actor,
    std::string_view target_room,
    Vec2i target);

[[nodiscard]] Result<std::vector<NavStep>> find_path(
    const WorldState& world,
    EntityId actor,
    Vec2i target);

[[nodiscard]] Result<bool> walk_to(
    WorldState& world,
    EntityId actor,
    std::string_view target_room,
    Vec2i target);

void advance_walking(WorldState& world, EntityId actor, int units_per_tick = 0);

void set_path(WorldState& world, EntityId actor, std::vector<NavStep> path);
void advance_navigation(WorldState& world, EntityId actor, Tick tick);

}  // namespace opennfh::simulation
