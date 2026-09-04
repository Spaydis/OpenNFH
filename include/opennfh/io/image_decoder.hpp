#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "opennfh/core/result.hpp"

namespace opennfh::io {

enum class ImageOrigin {
    BottomLeft,
    BottomRight,
    TopLeft,
    TopRight,
};

struct ImageInfo {
    std::uint16_t width{0};
    std::uint16_t height{0};
    std::uint8_t pixel_depth{0};
    std::uint8_t descriptor{0};
    ImageOrigin origin{ImageOrigin::TopLeft};
};

struct ImageRgba8 {
    ImageInfo info;
    std::vector<std::uint8_t> rgba;
};

[[nodiscard]] Result<ImageRgba8> decode_tga(std::span<const std::byte> bytes);
[[nodiscard]] Result<ImageRgba8> decode_png(std::span<const std::byte> bytes);

}  // namespace opennfh::io
