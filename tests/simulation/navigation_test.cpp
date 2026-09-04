#include <cassert>
#include <string>

#include "opennfh/simulation/navigation.hpp"

namespace {

opennfh::simulation::WorldState make_world() {
    using namespace opennfh;
    using namespace opennfh::content;
    using namespace opennfh::simulation;

    WorldState world;
    world.level.rooms = {
        Room{
            "start", {}, {}, {}, {},
            {Door{"start/cheap", 2, {10, 10}, true}, Door{"start/expensive", 2, {20, 10}, true}},
            {
                NeighborLink{"cheap", 2, "start/cheap", "cheap/start"},
                NeighborLink{"expensive", 8, "start/expensive", "expensive/start"},
            }, {}, {},
        },
        Room{
            "cheap", {}, {}, {}, {},
            {Door{"cheap/goal", 2, {30, 10}, true}},
            {NeighborLink{"goal", 2, "cheap/goal", "goal/cheap"}}, {}, {},
        },
        Room{
            "expensive", {}, {}, {}, {},
            {Door{"expensive/goal", 2, {40, 10}, true}},
            {NeighborLink{"goal", 8, "expensive/goal", "goal/expensive"}}, {}, {},
        },
        Room{
            "goal", {}, {}, {}, {}, {}, {}, {}, {},
        },
    };
    world.entities.push_back(EntityState{1, "woody", "start", {0, 0}, 4, true});
    return world;
}

}  // namespace

int main() {
    auto world = make_world();
    const auto path = opennfh::simulation::find_path(world, 1, "goal", {50, 50});
    assert(path.has_value());
    assert(path.value().size() == 2);
    assert(path.value()[0].door == "start/cheap");
    assert(path.value()[1].door == "cheap/goal");
    assert(path.value().back().destination.x == 50);
    assert(path.value().back().destination.y == 50);

    world.blocked_doors.insert("start/cheap");
    const auto alternate = opennfh::simulation::find_path(world, 1, "goal", {50, 50});
    assert(alternate.has_value());
    assert(alternate.value()[0].door == "start/expensive");

    world.blocked_doors.insert("start/expensive");
    const auto missing = opennfh::simulation::find_path(world, 1, "goal", {50, 50});
    assert(!missing.has_value());
    assert(missing.error().code == opennfh::ErrorCode::Missing);

    auto same_room = make_world();
    const auto local = opennfh::simulation::find_path(same_room, 1, {7, 8});
    assert(local.has_value());
    assert(local.value().size() == 1);
    assert(local.value()[0].door.empty());
    assert(local.value()[0].destination.x == 7);
    assert(local.value()[0].destination.y == 8);
    return 0;
}
