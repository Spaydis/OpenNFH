#include "opennfh/simulation/progression.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace opennfh::simulation {

Result<LevelResult> evaluate_level(const WorldState& world, const ProgressionState& progression) {
    (void)progression;
    int total_quota = 0;
    for (const auto& [name, quota] : world.quotas) {
        (void)name;
        total_quota += std::max(quota, 0);
    }
    const int minimum = std::max(world.level.meta.min_quota, 0);
    if (total_quota < minimum || (minimum == 0 && total_quota == 0)) {
        return Result<LevelResult>::success(LevelResult::Failed);
    }
    if (minimum > 0 && total_quota >= minimum * 3) {
        return Result<LevelResult>::success(LevelResult::Gold);
    }
    if (minimum > 0 && total_quota >= minimum * 2) {
        return Result<LevelResult>::success(LevelResult::Silver);
    }
    return Result<LevelResult>::success(LevelResult::Bronze);
}

void apply_level_result(ProgressionState& progression, std::string_view resource_id, LevelResult result) {
    if (result != LevelResult::Failed) {
        progression.levels[std::string(resource_id)] = content::LevelState::Completed;
    }
}

}  // namespace opennfh::simulation
