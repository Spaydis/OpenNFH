#include "opennfh/io/image_decoder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace opennfh::io {

namespace {

std::uint8_t byte_at(std::span<const std::byte> bytes, std::size_t index) {
    return std::to_integer<std::uint8_t>(bytes[index]);
}

std::uint16_t little_u16(std::span<const std::byte> bytes, std::size_t index) {
    return static_cast<std::uint16_t>(byte_at(bytes, index)) |
           static_cast<std::uint16_t>(byte_at(bytes, index + 1) << 8);
}

Error error(ErrorCode code, std::string message) {
    Error result;
    result.code = code;
    result.message = std::move(message);
    return result;
}

ImageOrigin origin_from_descriptor(std::uint8_t descriptor) {
    switch ((descriptor >> 4) & 0x3) {
    case 0:
        return ImageOrigin::BottomLeft;
    case 1:
        return ImageOrigin::BottomRight;
    case 2:
        return ImageOrigin::TopLeft;
    default:
        return ImageOrigin::TopRight;
    }
}

struct Rgba {
    std::uint8_t red{0};
    std::uint8_t green{0};
    std::uint8_t blue{0};
    std::uint8_t alpha{255};
};

Result<Rgba> read_pixel(std::span<const std::byte> bytes, std::size_t& cursor,
                        std::uint8_t depth, std::uint8_t descriptor) {
    const std::size_t bytes_per_pixel = depth / 8;
    if (cursor > bytes.size() || bytes.size() - cursor < bytes_per_pixel) {
        return Result<Rgba>::failure(error(ErrorCode::Format, "truncated TGA pixel data"));
    }
    Rgba pixel;
    if (depth == 16) {
        const auto value = little_u16(bytes, cursor);
        cursor += 2;
        if ((descriptor & 0x0f) >= 4) {
            const auto alpha4 = static_cast<std::uint8_t>((value >> 12) & 0x0f);
            const auto alpha = static_cast<std::uint8_t>(alpha4 * 17);
            pixel.alpha = alpha;
            if (alpha == 0) {
                pixel.red = 0;
                pixel.green = 0;
                pixel.blue = 0;
            } else {
                const auto unpremultiply = [alpha](std::uint8_t channel4) {
                    const auto premultiplied = static_cast<int>(channel4) * 17;
                    return static_cast<std::uint8_t>(std::min(
                        255, (premultiplied * 255 + alpha / 2) / alpha));
                };
                pixel.red = unpremultiply(static_cast<std::uint8_t>((value >> 8) & 0x0f));
                pixel.green = unpremultiply(static_cast<std::uint8_t>((value >> 4) & 0x0f));
                pixel.blue = unpremultiply(static_cast<std::uint8_t>(value & 0x0f));
            }
        } else {
            const auto red5 = static_cast<std::uint8_t>((value >> 11) & 0x1f);
            const auto green6 = static_cast<std::uint8_t>((value >> 5) & 0x3f);
            const auto blue5 = static_cast<std::uint8_t>(value & 0x1f);
            pixel.red = static_cast<std::uint8_t>((red5 << 3) | (red5 >> 2));
            pixel.green = static_cast<std::uint8_t>((green6 << 2) | (green6 >> 4));
            pixel.blue = static_cast<std::uint8_t>((blue5 << 3) | (blue5 >> 2));
            pixel.alpha = 255;
        }
    } else {
        pixel.blue = byte_at(bytes, cursor);
        pixel.green = byte_at(bytes, cursor + 1);
        pixel.red = byte_at(bytes, cursor + 2);
        pixel.alpha = depth == 32 ? byte_at(bytes, cursor + 3) : 255;
        cursor += bytes_per_pixel;
    }
    return Result<Rgba>::success(pixel);
}

void store_pixel(ImageRgba8& image, std::size_t source_index, Rgba pixel) {
    const auto width = image.info.width;
    const auto height = image.info.height;
    const auto source_x = source_index % width;
    const auto source_y = source_index / width;
    std::size_t destination_x = source_x;
    std::size_t destination_y = source_y;
    switch (image.info.origin) {
    case ImageOrigin::BottomLeft:
        destination_y = height - 1 - source_y;
        break;
    case ImageOrigin::BottomRight:
        destination_x = width - 1 - source_x;
        destination_y = height - 1 - source_y;
        break;
    case ImageOrigin::TopLeft:
        break;
    case ImageOrigin::TopRight:
        destination_x = width - 1 - source_x;
        break;
    }
    const std::size_t destination = (destination_y * width + destination_x) * 4;
    image.rgba[destination] = pixel.red;
    image.rgba[destination + 1] = pixel.green;
    image.rgba[destination + 2] = pixel.blue;
    image.rgba[destination + 3] = pixel.alpha;
}

}  // namespace

Result<ImageRgba8> decode_tga(std::span<const std::byte> bytes) {
    if (bytes.size() < 18) {
        return Result<ImageRgba8>::failure(error(ErrorCode::Format, "TGA header is truncated"));
    }
    const auto id_length = byte_at(bytes, 0);
    const auto color_map_type = byte_at(bytes, 1);
    const auto image_type = byte_at(bytes, 2);
    const auto width = little_u16(bytes, 12);
    const auto height = little_u16(bytes, 14);
    const auto depth = byte_at(bytes, 16);
    const auto descriptor = byte_at(bytes, 17);
    if (color_map_type != 0) {
        return Result<ImageRgba8>::failure(error(ErrorCode::Unsupported, "color-mapped TGA is unsupported"));
    }
    if (image_type != 2 && image_type != 10) {
        return Result<ImageRgba8>::failure(error(ErrorCode::Unsupported, "TGA image type is unsupported"));
    }
    if (width == 0 || height == 0 || (depth != 16 && depth != 24 && depth != 32)) {
        return Result<ImageRgba8>::failure(error(ErrorCode::Format, "invalid TGA dimensions or pixel depth"));
    }
    const std::size_t pixel_count = static_cast<std::size_t>(width) * height;
    if (pixel_count > std::numeric_limits<std::size_t>::max() / 4 || 18 + id_length > bytes.size()) {
        return Result<ImageRgba8>::failure(error(ErrorCode::Format, "TGA image is too large or truncated"));
    }

    ImageRgba8 image;
    image.info = ImageInfo{width, height, depth, descriptor, origin_from_descriptor(descriptor)};
    image.rgba.resize(pixel_count * 4);
    std::size_t cursor = 18 + id_length;
    std::size_t output_index = 0;
    while (output_index < pixel_count) {
        std::size_t repetitions = 1;
        bool run = false;
        if (image_type == 10) {
            if (cursor >= bytes.size()) {
                return Result<ImageRgba8>::failure(error(ErrorCode::Format, "truncated TGA RLE packet"));
            }
            const auto packet = byte_at(bytes, cursor++);
            repetitions = static_cast<std::size_t>((packet & 0x7f) + 1);
            run = (packet & 0x80) != 0;
            if (repetitions > pixel_count - output_index) {
                return Result<ImageRgba8>::failure(error(ErrorCode::Format, "TGA RLE packet exceeds image size"));
            }
        }
        if (run) {
            const auto pixel = read_pixel(bytes, cursor, depth, descriptor);
            if (!pixel.has_value()) {
                return Result<ImageRgba8>::failure(pixel.error());
            }
            for (std::size_t count = 0; count < repetitions; ++count) {
                store_pixel(image, output_index++, pixel.value());
            }
        } else {
            for (std::size_t count = 0; count < repetitions; ++count) {
                const auto pixel = read_pixel(bytes, cursor, depth, descriptor);
                if (!pixel.has_value()) {
                    return Result<ImageRgba8>::failure(pixel.error());
                }
                store_pixel(image, output_index++, pixel.value());
            }
        }
    }
    return Result<ImageRgba8>::success(std::move(image));
}

}  // namespace opennfh::io
