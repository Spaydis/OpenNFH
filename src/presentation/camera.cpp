#include "opennfh/presentation/camera.hpp"

#include <algorithm>
#include <limits>

namespace opennfh::presentation {
namespace {

Vec2i add(Vec2i left, Vec2i right) {
    return {left.x + right.x, left.y + right.y};
}

void include_point(Vec2i point, Vec2i& minimum, Vec2i& maximum) {
    minimum.x = std::min(minimum.x, point.x);
    minimum.y = std::min(minimum.y, point.y);
    maximum.x = std::max(maximum.x, point.x);
    maximum.y = std::max(maximum.y, point.y);
}

Vec2i entity_world_position(const simulation::WorldState& world,
                            const simulation::EntityState& entity) {
    for (const auto& room : world.level.rooms) {
        if (room.id == entity.room) return add(entity.position, room.offset);
    }
    return entity.position;
}

void clamp_offset(CameraState& camera) {
    camera.offset.x = std::clamp(camera.offset.x, camera.min_offset.x, camera.max_offset.x);
    camera.offset.y = std::clamp(camera.offset.y, camera.min_offset.y, camera.max_offset.y);
}

}  // namespace

CameraState make_camera(const simulation::WorldState& world, Vec2i viewport,
                        simulation::EntityId focus) {
    CameraState camera;
    camera.viewport = {
        std::max(viewport.x, 1),
        std::max(viewport.y, 1),
    };
    camera.focus = focus;

    Vec2i minimum{0, 0};
    Vec2i maximum{
        std::max(world.level.meta.size.x, 0),
        std::max(world.level.meta.size.y, 0),
    };
    for (const auto& room : world.level.rooms) {
        include_point(add(room.offset, room.path1), minimum, maximum);
        include_point(add(room.offset, room.path2), minimum, maximum);
        for (const auto& floor : room.floors) {
            include_point(add(room.offset, floor.offset), minimum, maximum);
            include_point(add(room.offset, {floor.offset.x + floor.size.x,
                                            floor.offset.y + floor.size.y}), minimum, maximum);
        }
        for (const auto& door : room.doors) {
            include_point(add(room.offset, door.position), minimum, maximum);
        }
    }
    for (const auto& entity : world.entities) {
        if (entity.active) include_point(entity_world_position(world, entity), minimum, maximum);
    }
    camera.min_offset = minimum;
    camera.max_offset = {
        std::max(minimum.x, maximum.x - camera.viewport.x),
        std::max(minimum.y, maximum.y - camera.viewport.y),
    };
    camera.offset = camera.min_offset;
    update_camera(camera, world);
    return camera;
}

void update_camera(CameraState& camera, const simulation::WorldState& world) {
    if (camera.follow_focus && camera.focus != 0) {
        for (const auto& entity : world.entities) {
            if (entity.id != camera.focus || !entity.active) continue;
            const auto position = entity_world_position(world, entity);
            camera.offset = {
                position.x - camera.viewport.x / 2,
                position.y - camera.viewport.y / 2,
            };
            break;
        }
    }
    clamp_offset(camera);
}

void scroll_camera(CameraState& camera, Vec2i delta) {
    camera.follow_focus = false;
    camera.offset = add(camera.offset, delta);
    clamp_offset(camera);
}

Vec2i to_camera_space(Vec2i world_position, const CameraState& camera) {
    return {
        world_position.x - camera.offset.x,
        world_position.y - camera.offset.y,
    };
}

}  // namespace opennfh::presentation
