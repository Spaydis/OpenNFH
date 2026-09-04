#include <cassert>
#include <string_view>
#include <vector>

#include "opennfh/simulation/input.hpp"

namespace {

opennfh::simulation::WorldState make_world() {
    using namespace opennfh;
    using namespace opennfh::content;
    using namespace opennfh::simulation;

    WorldState world;
    world.entities = {
        EntityState{1, "woody", "room", {0, 0}, 4, true},
        EntityState{2, "device", "room", {20, 20}, 1, true},
        EntityState{3, "device", "room", {30, 20}, 1, true},
        EntityState{4, "other", "room", {40, 20}, 1, true},
    };
    ObjectDef device;
    device.name = "device";
    device.standard_actions = {"missing", "use"};
    device.actions.push_back(ActionDef{
        "use", "woody", "use", "idle", "open", "idle", "2", 0, {}, {}, false,
    });
    world.level.objects.emplace("device", std::move(device));
    return world;
}

}  // namespace

int main() {
    using namespace opennfh::simulation;

    const std::vector<HitRegion> regions = {
        HitRegion{1, {0, 0}, {20, 20}, 1, 10, 0, true},
        HitRegion{2, {0, 0}, {20, 20}, 2, 5, 0, true},
        HitRegion{3, {0, 0}, {20, 20}, 2, 10, 0, true},
        HitRegion{4, {0, 0}, {20, 20}, 2, 10, 1, true},
    };
    const auto target = hit_test(regions, {10, 10});
    assert(target.has_value());
    assert(target.value() == 4);
    const auto outside = hit_test(regions, {40, 40});
    assert(!outside.has_value());

    auto world = make_world();
    const auto explicit_target = resolve_target(world, "entity:2");
    assert(explicit_target.has_value());
    assert(explicit_target.value() == 2);
    const auto named_target = resolve_target(world, "device");
    assert(named_target.has_value());
    assert(named_target.value() == 2);

    const auto selected = action_request_for(world, 1, 2);
    assert(selected.has_value());
    assert(selected.value().action_name == "use");
    const auto explicit_action = action_request_for(world, 1, 2, "use");
    assert(explicit_action.has_value());
    assert(explicit_action.value().target == 2);

    const auto rejected = action_request_for(world, 1, 3, "missing");
    assert(!rejected.has_value());
    assert(world.busy_entities.empty());
    assert(world.flags.empty());
    return 0;
}
