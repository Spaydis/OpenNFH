#include "opennfh/presentation/renderer.hpp"

#include <algorithm>
#include <utility>

namespace opennfh::presentation {

const io::ImageRgba8* AssetCache::find(std::string_view asset_id) const noexcept {
    (void)asset_id;
    return nullptr;
}

std::vector<RenderItem> sort_render_items(std::span<const RenderItem> items) {
    std::vector<RenderItem> sorted(items.begin(), items.end());
    std::stable_sort(sorted.begin(), sorted.end(), [](const RenderItem& left, const RenderItem& right) {
        if (left.layer != right.layer) {
            return left.layer < right.layer;
        }
        if (left.y_order != right.y_order) {
            return left.y_order < right.y_order;
        }
        return left.source_order < right.source_order;
    });
    return sorted;
}

void render_frame(SDL_Renderer* renderer, const RenderSnapshot& snapshot,
                  const AssetCache& assets, const ViewportTransform& transform) {
    (void)snapshot;
    (void)assets;
    (void)transform;
    if (renderer == nullptr) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

}  // namespace opennfh::presentation
