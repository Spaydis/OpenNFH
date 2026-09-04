#pragma once

#include "opennfh/simulation/world.hpp"

namespace opennfh::simulation {

[[nodiscard]] WorldState make_world(content::LevelDefinition level);

}  // namespace opennfh::simulation
