#pragma once

#include "opennfh/core/types.hpp"
#include "opennfh/simulation/world.hpp"

namespace opennfh::presentation {

struct CameraState {
    Vec2i viewport{800, 600};
    Vec2i offset;
    Vec2i min_offset;
    Vec2i max_offset;
    simulation::EntityId focus{0};
    bool follow_focus{true};
};

[[nodiscard]] CameraState make_camera(
    const simulation::WorldState& world,
    Vec2i viewport,
    simulation::EntityId focus = 0);

void update_camera(CameraState& camera, const simulation::WorldState& world);
void scroll_camera(CameraState& camera, Vec2i delta);
[[nodiscard]] Vec2i to_camera_space(Vec2i world_position, const CameraState& camera);

}  // namespace opennfh::presentation
