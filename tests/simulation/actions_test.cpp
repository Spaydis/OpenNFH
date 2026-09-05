#include <cassert>

#include "opennfh/simulation/actions.hpp"

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
    device.kind = "object";
    device.animations["open"] = AnimationDef{"open", "oneshot", {
        FrameDef{"open_0", {}}, FrameDef{"open_1", {}}, FrameDef{"open_2", {}}, FrameDef{"open_3", {}},
    }, {}};
    device.actions.push_back(ActionDef{
        "use", "woody", "use", "idle", "open", "idle", "3", 2, {}, {}, false,
    });
    world.level.objects["device"] = std::move(device);
    return world;
}

}  // namespace

int main() {
    auto world = make_world();
    const auto started = opennfh::simulation::begin_action(world, {1, 2, "use"}, 10);
    assert(started.has_value());
    assert(started.value().duration == 3);
    assert(started.value().noise == 2);
    assert(world.busy_entities.contains(1));
    assert(world.entities[0].animation == "use");
    assert(world.entities[1].animation == "open");

    auto transaction = started.value();
    opennfh::simulation::advance_action(world, transaction, 12);
    assert(world.busy_entities.contains(1));
    opennfh::simulation::advance_action(world, transaction, 13);
    assert(!world.busy_entities.contains(1));
    assert(world.entities[0].animation == "idle");
    assert(world.entities[1].animation == "idle");
    assert(world.emitted_noise.size() == 1);
    assert(world.emitted_noise[0].level == 2);

    auto grouped = make_world();
    grouped.level.objects.at("device").gfx = "device_graphics";
    grouped.level.objects.at("device").actions[0].time = "auto";
    grouped.level.objects.at("device").animations.clear();
    opennfh::content::ObjectDef graphics;
    graphics.name = "device_graphics";
    graphics.animations["open"] = opennfh::content::AnimationDef{
        "open", "oneshot", {
            {"open_0", {}}, {"open_1", {}}, {"open_2", {}},
            {"open_3", {}}, {"open_4", {}},
        }, {}};
    grouped.level.objects.emplace("device_graphics", std::move(graphics));
    const auto grouped_action = opennfh::simulation::begin_action(
        grouped, {1, 2, "use"}, 0);
    assert(grouped_action.has_value());
    assert(grouped_action.value().duration == 5);

    auto automatic = make_world();
    automatic.level.objects["device"].actions[0].time = "auto";
    const auto automatic_action = opennfh::simulation::begin_action(automatic, {1, 2, "use"}, 0);
    assert(automatic_action.has_value());
    assert(automatic_action.value().duration == 4);

    const auto rejected = opennfh::simulation::begin_action(world, {1, 99, "use"}, 0);
    assert(!rejected.has_value());
    assert(rejected.error().code == opennfh::ErrorCode::Missing);
    return 0;
}
