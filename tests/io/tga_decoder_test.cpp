#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "opennfh/io/image_decoder.hpp"

namespace {

void append_u16(std::vector<std::byte>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::byte>(value & 0xff));
    bytes.push_back(static_cast<std::byte>(value >> 8));
}

std::vector<std::byte> header(std::uint8_t type, std::uint16_t width, std::uint16_t height,
                              std::uint8_t depth, std::uint8_t descriptor) {
    std::vector<std::byte> bytes(18);
    bytes[2] = static_cast<std::byte>(type);
    bytes[12] = static_cast<std::byte>(width & 0xff);
    bytes[13] = static_cast<std::byte>(width >> 8);
    bytes[14] = static_cast<std::byte>(height & 0xff);
    bytes[15] = static_cast<std::byte>(height >> 8);
    bytes[16] = static_cast<std::byte>(depth);
    bytes[17] = static_cast<std::byte>(descriptor);
    return bytes;
}

std::uint8_t pixel(const opennfh::io::ImageRgba8& image, std::size_t index) {
    return image.rgba[index];
}

}  // namespace

int main() {
    auto sixteen = header(2, 1, 1, 16, 0x24);
    append_u16(sixteen, 0x003f);
    const auto red = opennfh::io::decode_tga(sixteen);
    assert(red.has_value());
    assert(red.value().info.width == 1);
    assert(red.value().info.height == 1);
    assert(red.value().info.pixel_depth == 16);
    assert(pixel(red.value(), 0) == 255);
    assert(pixel(red.value(), 1) == 0);
    assert(pixel(red.value(), 2) == 0);
    assert(pixel(red.value(), 3) == 255);

    auto thirty_two = header(2, 1, 1, 32, 0x08);
    thirty_two.push_back(static_cast<std::byte>(0));
    thirty_two.push_back(static_cast<std::byte>(0));
    thirty_two.push_back(static_cast<std::byte>(255));
    thirty_two.push_back(static_cast<std::byte>(128));
    const auto alpha = opennfh::io::decode_tga(thirty_two);
    assert(alpha.has_value());
    assert(pixel(alpha.value(), 0) == 255);
    assert(pixel(alpha.value(), 3) == 128);

    auto rle = header(10, 2, 1, 32, 0x08);
    rle.push_back(static_cast<std::byte>(0x81));
    rle.push_back(static_cast<std::byte>(0));
    rle.push_back(static_cast<std::byte>(255));
    rle.push_back(static_cast<std::byte>(0));
    rle.push_back(static_cast<std::byte>(255));
    const auto repeated = opennfh::io::decode_tga(rle);
    assert(repeated.has_value());
    assert(repeated.value().rgba.size() == 8);
    assert(pixel(repeated.value(), 1) == 255);
    assert(pixel(repeated.value(), 5) == 255);
    return 0;
}
