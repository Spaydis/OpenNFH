#include <cassert>

#include "opennfh/simulation/neighbor_ai.hpp"

int main() {
    using namespace opennfh::content;
    using namespace opennfh::simulation;

    WorldState world;
    world.entities = {
        EntityState{1, "woody", "room", {0, 0}, 4, true},
        EntityState{2, "neighbor", "room", {10, 0}, 4, true},
        EntityState{3, "banana", "room", {5, 0}, 1, true},
    };
    world.level.behaviors.push_back(BehaviorDef{
        "neighbor",
        "alarm",
        {
            TriggerRule{"banana", "nearobj", "once"},
            TriggerRule{{}, "room", "always"},
        },
    });

    dispatch_noise(world, NoiseEvent{1, 2, "room", 0}, 0);
    assert(world.emitted_triggers.size() == 2);
    assert(world.emitted_triggers[0].behavior == "alarm");
    assert(world.emitted_triggers[0].position == "nearobj");
    dispatch_noise(world, NoiseEvent{1, 2, "room", 1}, 1);
    assert(world.emitted_triggers.size() == 3);
    assert(mark_once_trigger(world, "alarm", "manual") == true);
    assert(mark_once_trigger(world, "alarm", "manual") == false);
    return 0;
}
