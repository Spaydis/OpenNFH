#include <cstdlib>
#include <iostream>
#include "opennfh/simulation/control.hpp"

void check(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

int main() {
    using namespace opennfh;
    using namespace opennfh::simulation;
    WorldState world;
    content::Room room;
    room.id = "lobby2";
    room.offset = {100, 200};
    room.path1 = {0, 30};
    room.path2 = {100, 30};
    room.floors.push_back({{0, 0}, {110, 50}, false, {}});
    world.level.rooms.push_back(room);
    world.entities = {{1, "woody", "lobby2", {0, 30}, 4, true},
                      {2, "cabinet", "lobby2", {60, 0}, 2, true}};
    content::ObjectDef actor;
    actor.kind = "actor";
    world.level.objects["woody"] = actor;
    content::ObjectDef cabinet;
    cabinet.kind = "object";
    cabinet.hotspots.push_back({"woody", {0, 30}});
    cabinet.standard_actions = {"open"};
    cabinet.actions.push_back({"open", "woody", "use", "ms3", "open", {}, "2"});
    cabinet.actions.push_back({"close", "woody", "use", "ms3", "ms", {}, "2"});
    world.level.objects["cabinet"] = cabinet;
    ControlState control;
    check(handle_click(world, control, 1, {140, 220}, 0).has_value(), "floor click rejected");
    update_control(world, control, 1);
    check(world.entities[0].position.x > 0 && world.entities[0].position.x < 40, "movement must be gradual");
    for (Tick tick=2; tick<20; ++tick) update_control(world, control, tick);
    check(world.entities[0].position.x == 40 && world.entities[0].position.y == 30, "floor projection wrong");
    check(handle_click(world, control, 1, {}, 2).has_value(), "object click rejected");
    for (Tick tick=20; tick<40; ++tick) update_control(world, control, tick);
    check(world.entities[0].position.x == 60, "actor did not approach object");
    check(world.entities[1].animation == "open", "object did not open after approach");
    check(handle_click(world, control, 1, {}, 2).has_value(), "close click rejected");
    for (Tick tick=40; tick<50; ++tick) update_control(world, control, tick);
    check(world.entities[1].animation == "ms", "second click did not close object");
    return 0;
}
