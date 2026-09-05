#include <cassert>

#include "opennfh/presentation/camera.hpp"

int main() {
    opennfh::simulation::WorldState world;
    world.level.meta.size = {1600, 1000};
    opennfh::content::Room room;
    room.id = "room";
    room.path1 = {0, 0};
    room.path2 = {1600, 1000};
    world.level.rooms.push_back(room);
    world.entities.push_back(
        opennfh::simulation::EntityState{1, "woody", "room", {800, 500}, 4, true});

    auto camera = opennfh::presentation::make_camera(world, {800, 600}, 1);
    opennfh::presentation::update_camera(camera, world);
    assert(camera.viewport.x == 800);
    assert(camera.viewport.y == 600);
    assert(camera.offset.x == 400);
    assert(camera.offset.y == 200);
    assert(opennfh::presentation::to_camera_space({800, 500}, camera).x == 400);
    assert(opennfh::presentation::to_camera_space({800, 500}, camera).y == 300);

    opennfh::presentation::scroll_camera(camera, {5000, 5000});
    assert(!camera.follow_focus);
    assert(camera.offset.x == camera.max_offset.x);
    assert(camera.offset.y == camera.max_offset.y);
    opennfh::presentation::scroll_camera(camera, {-5000, -5000});
    assert(camera.offset.x == camera.min_offset.x);
    assert(camera.offset.y == camera.min_offset.y);

    camera.follow_focus = true;
    world.entities[0].position = {100, 100};
    opennfh::presentation::update_camera(camera, world);
    assert(camera.offset.x == camera.min_offset.x);
    assert(camera.offset.y == camera.min_offset.y);
    return 0;
}
