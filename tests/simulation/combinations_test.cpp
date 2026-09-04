#include <algorithm>
#include <cassert>

#include "opennfh/simulation/combinations.hpp"

int main() {
    using namespace opennfh::content;
    using namespace opennfh::simulation;

    WorldState world;
    world.inventory = {"wire", "banana"};
    world.level.combinations.push_back(Combination{
        "trap",
        true,
        false,
        {Ingredient{"wire", true}, Ingredient{"banana", false}},
    });

    assert(apply_combination(world, "trap"));
    assert(std::find(world.inventory.begin(), world.inventory.end(), "wire") == world.inventory.end());
    assert(std::find(world.inventory.begin(), world.inventory.end(), "banana") != world.inventory.end());
    assert(world.quotas["trap"] == 1);
    assert(!apply_combination(world, "trap"));
    return 0;
}
