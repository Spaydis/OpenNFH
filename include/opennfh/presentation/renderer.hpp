#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "opennfh/io/image_decoder.hpp"
#include "opennfh/presentation/viewport.hpp"
#include "opennfh/simulation/world.hpp"

namespace opennfh::presentation {

struct RenderItem {
    simulation::EntityId entity{0};
    std::string asset_id;
    Vec2i position;
    int layer{0};
    int y_order{0};
    std::uint64_t source_order{0};
};

struct RenderSnapshot {
    Vec2i logical_size;
    std::vector<RenderItem> items;
};

using PresentationSnapshot = RenderSnapshot;

class AssetCache {
public:
    [[nodiscard]] const io::ImageRgba8* find(std::string_view asset_id) const noexcept;
};

[[nodiscard]] std::vector<RenderItem> sort_render_items(std::span<const RenderItem> items);
void render_frame(SDL_Renderer* renderer, const RenderSnapshot& snapshot,
                  const AssetCache& assets, const ViewportTransform& transform);

}  // namespace opennfh::presentation
