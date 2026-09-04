#include "opennfh/presentation/renderer.hpp"

#include <algorithm>
#include <utility>

namespace opennfh::presentation {

AssetCache::~AssetCache() {
    release_all_textures();
}

void AssetCache::insert(std::string asset_id, io::ImageRgba8 image) {
    if (asset_id.empty()) {
        return;
    }
    const auto texture = textures_.find(asset_id);
    if (texture != textures_.end()) {
        SDL_DestroyTexture(texture->second.texture);
        textures_.erase(texture);
    }
    images_[std::move(asset_id)] = std::move(image);
}

const io::ImageRgba8* AssetCache::find(std::string_view asset_id) const {
    const auto found = images_.find(std::string(asset_id));
    return found == images_.end() ? nullptr : &found->second;
}

SDL_Texture* AssetCache::texture_for(
    SDL_Renderer* renderer,
    std::string_view asset_id,
    const io::ImageRgba8& image) const {
    const auto cached = textures_.find(std::string(asset_id));
    if (cached != textures_.end()) {
        if (cached->second.renderer == renderer) {
            return cached->second.texture;
        }
        SDL_DestroyTexture(cached->second.texture);
        textures_.erase(cached);
    }

    SDL_Texture* texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
        static_cast<int>(image.info.width), static_cast<int>(image.info.height));
    if (texture == nullptr) {
        return nullptr;
    }
    if (!SDL_UpdateTexture(texture, nullptr, image.rgba.data(),
                           static_cast<int>(image.info.width) * 4)) {
        SDL_DestroyTexture(texture);
        return nullptr;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    textures_.emplace(std::string(asset_id), TextureEntry{renderer, texture});
    return texture;
}

void AssetCache::release_renderer(SDL_Renderer* renderer) noexcept {
    for (auto texture = textures_.begin(); texture != textures_.end();) {
        if (texture->second.renderer == renderer) {
            SDL_DestroyTexture(texture->second.texture);
            texture = textures_.erase(texture);
        } else {
            ++texture;
        }
    }
}

void AssetCache::release_all_textures() noexcept {
    for (const auto& [asset_id, texture] : textures_) {
        (void)asset_id;
        SDL_DestroyTexture(texture.texture);
    }
    textures_.clear();
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
    if (renderer == nullptr) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    for (const auto& item : sort_render_items(snapshot.items)) {
        const auto* image = assets.find(item.asset_id);
        if (image == nullptr || image->info.width == 0 || image->info.height == 0 ||
            image->rgba.empty()) {
            continue;
        }
        SDL_Texture* texture = assets.texture_for(renderer, item.asset_id, *image);
        if (texture == nullptr) {
            continue;
        }

        const auto top_left = transform.to_screen(item.position);
        const auto bottom_right = transform.to_screen({
            item.position.x + static_cast<int>(image->info.width),
            item.position.y + static_cast<int>(image->info.height),
        });
        SDL_FRect destination{
            static_cast<float>(top_left.x),
            static_cast<float>(top_left.y),
            static_cast<float>(bottom_right.x - top_left.x),
            static_cast<float>(bottom_right.y - top_left.y),
        };
        SDL_RenderTexture(renderer, texture, nullptr, &destination);
    }
    SDL_RenderPresent(renderer);
}

}  // namespace opennfh::presentation
