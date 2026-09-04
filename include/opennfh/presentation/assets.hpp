#pragma once

#include <string_view>

#include "opennfh/core/result.hpp"
#include "opennfh/io/data_root.hpp"
#include "opennfh/io/image_decoder.hpp"
#include "opennfh/presentation/renderer.hpp"

namespace opennfh::presentation {

[[nodiscard]] Result<io::ImageRgba8> load_entity_image(
    const io::DataRoot& root,
    const simulation::WorldState& world,
    simulation::EntityId entity);

[[nodiscard]] RenderSnapshot make_render_snapshot(
    const simulation::WorldState& world);

}  // namespace opennfh::presentation
