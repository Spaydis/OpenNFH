#pragma once

#include <string_view>

#include "opennfh/simulation/world.hpp"

namespace opennfh::simulation {

void dispatch_noise(WorldState& world, const NoiseEvent& event, Tick now);
void update_neighbor_ai(WorldState& world, Tick now);
[[nodiscard]] bool mark_once_trigger(WorldState& world, std::string_view behavior, std::string_view trigger_key);

}  // namespace opennfh::simulation
