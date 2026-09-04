#pragma once

#include "opennfh/core/types.hpp"

namespace opennfh::presentation {

struct ViewportConfig {
    Vec2i logical_size;
    int window_width{0};
    int window_height{0};
    bool integer_scale{false};
};

struct ViewportTransform {
    Vec2i logical_size;
    Vec2i window_size;
    Vec2i offset;
    double scale{1.0};

    [[nodiscard]] Vec2i to_screen(Vec2i logical) const;
    [[nodiscard]] Vec2i to_logical(Vec2i screen) const;
};

[[nodiscard]] ViewportTransform make_viewport(ViewportConfig config);

}  // namespace opennfh::presentation
