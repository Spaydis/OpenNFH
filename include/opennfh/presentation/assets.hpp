#pragma once

#include <string_view>
#include <vector>

#include "opennfh/core/result.hpp"
#include "opennfh/io/data_root.hpp"
#include "opennfh/io/image_decoder.hpp"
#include "opennfh/presentation/renderer.hpp"
#include "opennfh/simulation/input.hpp"

namespace opennfh::presentation {

[[nodiscard]] Result<io::ImageRgba8> load_entity_image(
    const io::DataRoot& root,
    const simulation::WorldState& world,
    simulation::EntityId entity, simulation::Tick tick = 0);

[[nodiscard]] RenderSnapshot make_render_snapshot(
    const simulation::WorldState& world,
    simulation::Tick tick = 0,
    Vec2i camera_offset = {},
    Vec2i viewport_size = {});

[[nodiscard]] Vec2i entity_world_position(const simulation::WorldState& world,
                                         const simulation::EntityState& entity);
[[nodiscard]] std::vector<simulation::HitRegion> make_hit_regions(
    const simulation::WorldState& world, const RenderSnapshot& snapshot,
    const AssetCache& assets, simulation::Tick tick = 0);

}  // namespace opennfh::presentation
