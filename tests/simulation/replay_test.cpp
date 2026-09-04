#include <cassert>
#include <sstream>

#include "opennfh/simulation/replay.hpp"

int main() {
    using namespace opennfh::simulation;

    const Replay input{
        1,
        {
            InputEvent{4, InputAction::PointerClick, {10, 20}, "device", "use"},
            InputEvent{9, InputAction::Pause, {0, 0}, {}},
        },
    };
    std::stringstream stream;
    write_replay(stream, input);
    const auto parsed = read_replay(stream);
    assert(parsed.has_value());
    assert(parsed.value().version == 1);
    assert(parsed.value().events.size() == 2);
    assert(parsed.value().events[0].target == "device");
    assert(parsed.value().events[0].action_name == "use");
    assert(parsed.value().events[1].action == InputAction::Pause);

    SimulationSnapshot snapshot;
    snapshot.tick = 9;
    snapshot.entities.push_back(EntityState{1, "woody", "room", {3, 4}, 4, true});
    const auto first_hash = hash_snapshot(snapshot);
    snapshot.quotas["trap"] = 1;
    assert(hash_snapshot(snapshot) != first_hash);
    return 0;
}
