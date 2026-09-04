#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "opennfh/io/image_decoder.hpp"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace opennfh::io {

namespace {

Error error(std::string message) {
    Error result;
    result.code = ErrorCode::Format;
    result.message = std::move(message);
    return result;
}

}  // namespace

Result<ImageRgba8> decode_png(std::span<const std::byte> bytes) {
    if (bytes.empty() || bytes.size() > static_cast<std::size_t>(INT_MAX)) {
        return Result<ImageRgba8>::failure(error("PNG input is empty or too large"));
    }

    int width = 0;
    int height = 0;
    int source_channels = 0;
    const auto* input = reinterpret_cast<const stbi_uc*>(bytes.data());
    stbi_uc* decoded = stbi_load_from_memory(input, static_cast<int>(bytes.size()), &width, &height,
                                              &source_channels, 4);
    if (decoded == nullptr || width <= 0 || height <= 0 || width > 65535 || height > 65535) {
        if (decoded != nullptr) {
            stbi_image_free(decoded);
        }
        return Result<ImageRgba8>::failure(error("PNG decode failed"));
    }

    const auto pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (pixel_count > std::numeric_limits<std::size_t>::max() / 4) {
        stbi_image_free(decoded);
        return Result<ImageRgba8>::failure(error("PNG image is too large"));
    }

    ImageRgba8 image;
    image.info = ImageInfo{
        static_cast<std::uint16_t>(width),
        static_cast<std::uint16_t>(height),
        32,
        0x08,
        ImageOrigin::TopLeft,
    };
    image.rgba.assign(decoded, decoded + pixel_count * 4);
    stbi_image_free(decoded);
    return Result<ImageRgba8>::success(std::move(image));
}

}  // namespace opennfh::io
