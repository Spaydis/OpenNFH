#include "opennfh/presentation/viewport.hpp"

#include <algorithm>
#include <cmath>

namespace opennfh::presentation {

ViewportTransform make_viewport(ViewportConfig config) {
    ViewportTransform transform;
    transform.logical_size = config.logical_size;
    transform.window_size = {config.window_width, config.window_height};
    if (config.logical_size.x <= 0 || config.logical_size.y <= 0 ||
        config.window_width <= 0 || config.window_height <= 0) {
        transform.scale = 1.0;
        return transform;
    }

    const double horizontal = static_cast<double>(config.window_width) / config.logical_size.x;
    const double vertical = static_cast<double>(config.window_height) / config.logical_size.y;
    transform.scale = std::min(horizontal, vertical);
    if (config.integer_scale) {
        transform.scale = std::max(std::floor(transform.scale), 1.0);
    }
    transform.offset = {
        static_cast<int>(std::lround((config.window_width - config.logical_size.x * transform.scale) / 2.0)),
        static_cast<int>(std::lround((config.window_height - config.logical_size.y * transform.scale) / 2.0)),
    };
    return transform;
}

Vec2i ViewportTransform::to_screen(Vec2i logical) const {
    return {
        offset.x + static_cast<int>(std::lround(logical.x * scale)),
        offset.y + static_cast<int>(std::lround(logical.y * scale)),
    };
}

Vec2i ViewportTransform::to_logical(Vec2i screen) const {
    return {
        static_cast<int>(std::lround((screen.x - offset.x) / scale)),
        static_cast<int>(std::lround((screen.y - offset.y) / scale)),
    };
}

}  // namespace opennfh::presentation
