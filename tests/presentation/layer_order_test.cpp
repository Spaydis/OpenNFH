#include <cassert>
#include <vector>

#include "opennfh/presentation/renderer.hpp"

int main() {
    using opennfh::presentation::RenderItem;
    const std::vector<RenderItem> items = {
        RenderItem{1, "front", {0, 0}, 2, 20, 1},
        RenderItem{2, "back", {0, 0}, 1, 99, 0},
        RenderItem{3, "same-y-first", {0, 0}, 2, 10, 2},
        RenderItem{4, "same-y-second", {0, 0}, 2, 10, 3},
    };
    const auto sorted = opennfh::presentation::sort_render_items(items);
    assert(sorted.size() == items.size());
    assert(sorted[0].entity == 2);
    assert(sorted[1].entity == 3);
    assert(sorted[2].entity == 4);
    assert(sorted[3].entity == 1);

    opennfh::presentation::AssetCache assets;
    opennfh::io::ImageRgba8 image;
    image.info = {1, 1, 32, 0, opennfh::io::ImageOrigin::TopLeft};
    image.rgba = {255, 255, 255, 255};
    assets.insert("pixel", image);
    assert(assets.size() == 1);
    assert(assets.find("pixel") != nullptr);
    assert(assets.find("missing") == nullptr);
    return 0;
}
