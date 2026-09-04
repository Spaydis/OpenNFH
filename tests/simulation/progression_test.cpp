#include <cassert>

#include "opennfh/simulation/progression.hpp"

int main() {
    using namespace opennfh::content;
    using namespace opennfh::simulation;

    WorldState world;
    world.level.meta.resource_id = "level_test";
    world.level.meta.min_quota = 2;
    world.quotas["trap"] = 2;

    ProgressionState progression;
    progression.levels["level_test"] = LevelState::Playable;
    const auto bronze = evaluate_level(world, progression);
    assert(bronze.has_value());
    assert(bronze.value() == LevelResult::Bronze);
    apply_level_result(progression, "level_test", bronze.value());
    assert(progression.levels["level_test"] == LevelState::Completed);

    world.quotas.clear();
    const auto failed = evaluate_level(world, progression);
    assert(failed.has_value());
    assert(failed.value() == LevelResult::Failed);
    return 0;
}
