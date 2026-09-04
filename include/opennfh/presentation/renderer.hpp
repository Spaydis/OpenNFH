#pragma once

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
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
    AssetCache() = default;
    ~AssetCache();

    AssetCache(const AssetCache&) = delete;
    AssetCache& operator=(const AssetCache&) = delete;
    AssetCache(AssetCache&&) = delete;
    AssetCache& operator=(AssetCache&&) = delete;

    void insert(std::string asset_id, io::ImageRgba8 image);
    [[nodiscard]] const io::ImageRgba8* find(std::string_view asset_id) const;
    [[nodiscard]] std::size_t size() const noexcept { return images_.size(); }

    // Call this before destroying the associated SDL renderer.
    void release_renderer(SDL_Renderer* renderer) noexcept;

    friend void render_frame(SDL_Renderer*, const RenderSnapshot&, const AssetCache&, const ViewportTransform&);

private:
    struct TextureEntry {
        SDL_Renderer* renderer{nullptr};
        SDL_Texture* texture{nullptr};
    };

    [[nodiscard]] SDL_Texture* texture_for(
        SDL_Renderer* renderer,
        std::string_view asset_id,
        const io::ImageRgba8& image) const;
    void release_all_textures() noexcept;

    std::map<std::string, io::ImageRgba8> images_;
    mutable std::map<std::string, TextureEntry> textures_;
};

[[nodiscard]] std::vector<RenderItem> sort_render_items(std::span<const RenderItem> items);
void render_frame(SDL_Renderer* renderer, const RenderSnapshot& snapshot,
                  const AssetCache& assets, const ViewportTransform& transform);

}  // namespace opennfh::presentation
