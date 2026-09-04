#pragma once

#include <map>
#include <string>
#include <string_view>

#include "opennfh/core/result.hpp"
#include "opennfh/simulation/world.hpp"

namespace opennfh::simulation {

enum class LevelResult {
    Failed,
    Bronze,
    Silver,
    Gold,
};

struct ProgressionState {
    std::map<std::string, content::LevelState> levels;
    std::map<std::string, int> quotas;
};

[[nodiscard]] Result<LevelResult> evaluate_level(
    const WorldState& world,
    const ProgressionState& progression);

void apply_level_result(ProgressionState& progression, std::string_view resource_id, LevelResult result);

}  // namespace opennfh::simulation
