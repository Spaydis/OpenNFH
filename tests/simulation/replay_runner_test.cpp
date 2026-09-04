#include <cassert>
#include <utility>

#include "opennfh/simulation/replay.hpp"

namespace {

opennfh::simulation::WorldState make_world() {
    using namespace opennfh;
    using namespace opennfh::content;
    using namespace opennfh::simulation;

    WorldState world;
    world.entities = {
        EntityState{1, "woody", "room", {0, 0}, 4, true},
        EntityState{2, "device", "room", {10, 0}, 1, true},
    };
    ObjectDef device;
    device.name = "device";
    device.standard_actions = {"use"};
    device.actions.push_back(ActionDef{
        "use", "woody", "use", "idle", "open", "idle", "2", 2, {}, {}, false,
    });
    world.level.objects.emplace("device", std::move(device));
    return world;
}

opennfh::simulation::Replay make_trace() {
    using namespace opennfh::simulation;
    return Replay{
        1,
        {
            InputEvent{0, InputAction::PointerClick, {10, 0}, "entity:2", "use"},
            InputEvent{2, InputAction::Pause, {0, 0}, {}, {}},
            InputEvent{3, InputAction::Pause, {0, 0}, {}, {}},
            InputEvent{4, InputAction::Quit, {0, 0}, {}, {}},
        },
    };
}

}  // namespace

int main() {
    using namespace opennfh::simulation;

    auto world = make_world();
    const auto trace = make_trace();
    const auto first = run_replay(world, trace, {1, 2, true});
    assert(first.has_value());
    assert(first.value().final_tick == 4);
    assert(first.value().processed_events == 4);
    assert(first.value().stopped_by_quit);
    assert(!first.value().paused);
    assert(world.emitted_noise.size() == 1);
    assert(world.emitted_noise[0].level == 2);

    auto second_world = make_world();
    const auto second = run_replay(second_world, trace, {1, 2, true});
    assert(second.has_value());
    assert(first.value().snapshot_hash == second.value().snapshot_hash);

    auto invalid_world = make_world();
    Replay invalid = trace;
    invalid.events[2].tick = 1;
    const auto invalid_result = run_replay(invalid_world, invalid, {1, 0, true});
    assert(!invalid_result.has_value());
    return 0;
}
