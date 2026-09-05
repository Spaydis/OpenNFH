#include <cassert>

#include "opennfh/simulation/control.hpp"

namespace {

opennfh::simulation::WorldState make_world() {
    using namespace opennfh;
    using namespace opennfh::content;
    using namespace opennfh::simulation;

    WorldState world;
    world.level.meta.size = {400, 200};
    Room source;
    source.id = "source";
    source.path1 = {0, 60};
    source.path2 = {200, 60};
    source.floors.push_back({{0, 0}, {220, 100}, false, {}});
    source.doors.push_back({"source/exit", 2, {100, 20}, true});
    source.neighbors.push_back({"dest", 1, "source/exit", "dest/entry"});
    world.level.rooms.push_back(source);

    Room destination;
    destination.id = "dest";
    destination.path1 = {-40, 70};
    destination.path2 = {100, 70};
    destination.floors.push_back({{-80, 0}, {220, 100}, false, {}});
    destination.doors.push_back({"dest/entry", 2, {-40, 30}, true});
    world.level.rooms.push_back(destination);

    world.entities = {
        EntityState{1, "woody", "source", {0, 60}, 4, true},
        EntityState{2, "source/exit", "source", {100, 20}, 2, true},
        EntityState{3, "dest/entry", "dest", {-40, 30}, 2, true},
    };

    ObjectDef source_door;
    source_door.name = "source/exit";
    source_door.kind = "door";
    source_door.gfx = "source/door_graphics";
    source_door.hotspots.push_back({"woody", {12, 40}});
    source_door.standard_actions = {"goto"};
    source_door.actions.push_back({"enter", "neighbor", "wrong_enter", "", "wrong", "wrong_next", "1"});
    source_door.actions.push_back({"enter", "woody", "inv", "", "enter", "ms", "1"});
    world.level.objects.emplace("source/exit", source_door);

    ObjectDef destination_door;
    destination_door.name = "dest/entry";
    destination_door.kind = "door";
    destination_door.gfx = "dest/door_graphics";
    destination_door.hotspots.push_back({"woody", {12, 40}});
    destination_door.standard_actions = {"goto"};
    destination_door.actions.push_back({"leave", "neighbor", "wrong_leave", "", "wrong", "wrong_next", "1"});
    destination_door.actions.push_back({"leave", "woody", "inv", "", "leave", "ms", "1"});
    world.level.objects.emplace("dest/entry", destination_door);
    return world;
}

}  // namespace

int main() {
    auto world = make_world();
    opennfh::simulation::ControlState control;
    const auto clicked = opennfh::simulation::handle_click(world, control, 1, {}, 2);
    assert(clicked.has_value());
    assert(control.door_traversal.has_value());
    assert(control.door_traversal->destination_room == "dest");
    assert(control.door_traversal->destination_position.x == -28);
    assert(control.door_traversal->destination_position.y == 70);

    for (opennfh::simulation::Tick tick = 1; tick <= 40; ++tick) {
        opennfh::simulation::update_control(world, control, tick);
    }
    assert(world.entities[0].room == "dest");
    assert(world.entities[0].position.x == -28);
    assert(world.entities[0].position.y == 70);
    assert(!control.door_traversal.has_value());
    assert(world.entities[1].animation == "ms");
    assert(world.entities[2].animation == "ms");

    auto blocked = make_world();
    blocked.blocked_doors.insert("source/exit");
    opennfh::simulation::ControlState blocked_control;
    const auto rejected = opennfh::simulation::handle_click(blocked, blocked_control, 1, {}, 2);
    assert(!rejected.has_value());
    assert(blocked.entities[0].position.x == 0);
    assert(!blocked_control.door_traversal.has_value());
    return 0;
}
