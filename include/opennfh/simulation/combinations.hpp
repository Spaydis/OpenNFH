#pragma once

#include <string_view>

#include "opennfh/simulation/world.hpp"

namespace opennfh::simulation {

[[nodiscard]] bool apply_combination(WorldState& world, std::string_view result_id);

}  // namespace opennfh::simulation
