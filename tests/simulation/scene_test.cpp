#include <cassert>
#include <utility>

#include "opennfh/simulation/scene.hpp"

int main() {
    opennfh::content::LevelDefinition level;
    opennfh::content::Room room;
    room.id = "room";
    room.actors.push_back({"woody", 4, {10, 20}, "idle"});
    room.objects.push_back({"device", 1, {30, 40}, true});
    room.doors.push_back({"room/exit", 2, {50, 60}, false});
    level.rooms.push_back(std::move(room));

    const auto world = opennfh::simulation::make_world(std::move(level));
    assert(world.entities.size() == 3);
    assert(world.entities[0].id == 1);
    assert(world.entities[0].kind == "woody");
    assert(world.entities[0].room == "room");
    assert(world.entities[0].position.x == 10);
    assert(world.entities[1].kind == "device");
    assert(world.entities[1].position.y == 40);
    assert(world.entities[1].active);
    assert(world.entities[2].kind == "room/exit");
    assert(!world.entities[2].active);
    return 0;
}
