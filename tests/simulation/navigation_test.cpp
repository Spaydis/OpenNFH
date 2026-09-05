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

    opennfh::simulation::WorldState aliased;
    aliased.level.rooms = {
        opennfh::content::Room{
            "fro", {}, {}, {}, {},
            {opennfh::content::Door{"fro/anc", 2, {10, 10}, true}},
            {opennfh::content::NeighborLink{"anc2", 2, "fro/anc", "anc/fro"}}, {}, {},
        },
        opennfh::content::Room{
            "anc2", {}, {}, {}, {},
            {opennfh::content::Door{"anc/fro", 2, {1, 10}, true}}, {}, {}, {},
        },
    };
    aliased.entities.push_back(
        opennfh::simulation::EntityState{1, "woody", "fro", {0, 10}, 4, true});
    opennfh::content::ObjectDef fro_door;
    fro_door.name = "fro/anc";
    fro_door.kind = "door";
    fro_door.hotspots.push_back({"woody", {12, 40}});
    aliased.level.objects.emplace("fro/anc", fro_door);
    opennfh::content::ObjectDef anc_door;
    anc_door.name = "anc/fro";
    anc_door.kind = "door";
    anc_door.hotspots.push_back({"woody", {7, 2}});
    aliased.level.objects.emplace("anc/fro", anc_door);
    const auto alias_path = opennfh::simulation::find_path(aliased, 1, "anc2", {20, 10});
    assert(alias_path.has_value());
    assert(alias_path.value().size() == 1);
    assert(alias_path.value()[0].room == "anc2");
    assert(alias_path.value()[0].door == "fro/anc");
    assert(alias_path.value()[0].arrival.x == 8);
    assert(alias_path.value()[0].arrival.y == 12);
    assert(alias_path.value()[0].destination.x == 20);

    const auto queued = opennfh::simulation::walk_to(aliased, 1, "anc2", {20, 10});
    assert(queued.has_value());
    opennfh::simulation::advance_walking(aliased, 1, 6);
    assert(aliased.entities[0].position.x > 0);
    for (int tick = 0; tick < 20; ++tick) {
        opennfh::simulation::advance_walking(aliased, 1, 6);
    }
    assert(aliased.entities[0].room == "anc2");
    assert(aliased.entities[0].position.x == 20);
    aliased.blocked_doors.insert("anc/fro");
    const auto blocked = opennfh::simulation::walk_to(aliased, 1, "fro", {0, 10});
    assert(!blocked.has_value());

    opennfh::simulation::WorldState hotspots;
    hotspots.level.rooms = {
        opennfh::content::Room{
            "source", {}, {}, {}, {},
            {opennfh::content::Door{"source/exit", 2, {100, 20}, true}},
            {opennfh::content::NeighborLink{"dest", 1, "source/exit", "dest/entry"}}, {}, {},
        },
        opennfh::content::Room{
            "dest", {}, {}, {}, {},
            {opennfh::content::Door{"dest/entry", 2, {-40, 30}, true}}, {}, {}, {},
        },
    };
    hotspots.entities.push_back(
        opennfh::simulation::EntityState{1, "woody", "source", {0, 60}, 4, true});
    opennfh::content::ObjectDef source_door;
    source_door.name = "source/exit";
    source_door.kind = "door";
    source_door.hotspots.push_back({"woody", {12, 40}});
    hotspots.level.objects.emplace("source/exit", source_door);
    opennfh::content::ObjectDef destination_door;
    destination_door.name = "dest/entry";
    destination_door.kind = "door";
    destination_door.hotspots.push_back({"woody", {12, 40}});
    hotspots.level.objects.emplace("dest/entry", destination_door);
    const auto hotspot_path = opennfh::simulation::find_path(
        hotspots, 1, "dest", {0, 70});
    assert(hotspot_path.has_value());
    assert(hotspot_path.value()[0].arrival.x == -28);
    assert(hotspot_path.value()[0].arrival.y == 70);
    const auto hotspot_walk = opennfh::simulation::walk_to(
        hotspots, 1, "dest", {0, 70});
    assert(hotspot_walk.has_value());
    assert(hotspots.pending_paths.at(1)[0].destination.x == 112);
    assert(hotspots.pending_paths.at(1)[0].destination.y == 60);
    return 0;
}
