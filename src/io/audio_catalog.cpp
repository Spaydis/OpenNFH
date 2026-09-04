#include "opennfh/io/audio_catalog.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace opennfh::io {

namespace {

std::uint8_t byte_at(std::span<const std::byte> bytes, std::size_t offset) {
    return std::to_integer<std::uint8_t>(bytes[offset]);
}

std::uint16_t little_u16(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
           static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1]) << 8);
}

std::uint32_t little_u32(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24);
}

bool has_text(std::span<const std::byte> bytes, std::size_t offset, const char* value) {
    for (std::size_t index = 0; value[index] != '\0'; ++index) {
        if (offset + index >= bytes.size() ||
            std::to_integer<unsigned char>(bytes[offset + index]) != static_cast<unsigned char>(value[index])) {
            return false;
        }
    }
    return true;
}

std::size_t id3_end(std::span<const std::byte> bytes, std::size_t offset) {
    if (!has_text(bytes, offset, "ID3") || offset + 10 > bytes.size()) {
        return 0;
    }
    const auto version = byte_at(bytes, offset + 3);
    if (version < 2 || version > 4) {
        return 0;
    }
    for (std::size_t index = 6; index < 10; ++index) {
        if ((byte_at(bytes, offset + index) & 0x80) != 0) {
            return 0;
        }
    }
    const auto size =
        (static_cast<std::uint32_t>(byte_at(bytes, offset + 6)) << 21) |
        (static_cast<std::uint32_t>(byte_at(bytes, offset + 7)) << 14) |
        (static_cast<std::uint32_t>(byte_at(bytes, offset + 8)) << 7) |
        static_cast<std::uint32_t>(byte_at(bytes, offset + 9));
    const auto footer_size = (byte_at(bytes, offset + 5) & 0x10) != 0 ? 10u : 0u;
    const auto total_size = static_cast<std::size_t>(10u + size + footer_size);
    if (total_size > bytes.size() - offset) {
        return 0;
    }
    return offset + total_size;
}

bool valid_mpeg_frame(std::span<const std::byte> bytes, std::size_t offset) {
    if (offset + 4 > bytes.size()) {
        return false;
    }
    const auto first = byte_at(bytes, offset);
    const auto second = byte_at(bytes, offset + 1);
    const auto third = byte_at(bytes, offset + 2);
    if (first != 0xff || (second & 0xe0) != 0xe0) {
        return false;
    }
    const auto version = static_cast<std::uint8_t>((second >> 3) & 0x03);
    const auto layer = static_cast<std::uint8_t>((second >> 1) & 0x03);
    const auto bitrate = static_cast<std::uint8_t>((third >> 4) & 0x0f);
    const auto sample_rate = static_cast<std::uint8_t>((third >> 2) & 0x03);
    return version != 1 && layer != 0 && bitrate != 0 && bitrate != 15 && sample_rate != 3;
}

bool has_frame_after(
    std::span<const std::byte> bytes,
    std::size_t start,
    std::size_t limit) {
    for (std::size_t offset = start; offset + 4 <= limit; ++offset) {
        if (valid_mpeg_frame(bytes, offset)) {
            return true;
        }
    }
    return false;
}

bool has_mp3_signature(std::span<const std::byte> bytes) {
    const auto limit = std::min<std::size_t>(bytes.size(), 1024 * 1024);
    for (std::size_t offset = 0; offset + 4 <= limit; ++offset) {
        if (valid_mpeg_frame(bytes, offset)) {
            return true;
        }
        const auto tag_end = id3_end(bytes, offset);
        if (tag_end != 0 && has_frame_after(bytes, tag_end, limit)) {
            return true;
        }
    }
    return false;
}
Error error(ErrorCode code, std::string message) {
    Error result;
    result.code = code;
    result.message = std::move(message);
    return result;
}

}  // namespace

Result<AudioSpec> inspect_audio(std::span<const std::byte> bytes) {
    if (bytes.size() >= 12 && has_text(bytes, 0, "RIFF") && has_text(bytes, 8, "WAVE")) {
        std::size_t cursor = 12;
        while (cursor + 8 <= bytes.size()) {
            const auto chunk_size = little_u32(bytes, cursor + 4);
            if (chunk_size > bytes.size() - cursor - 8) {
                return Result<AudioSpec>::failure(error(ErrorCode::Format, "truncated WAV chunk"));
            }
            if (has_text(bytes, cursor, "fmt ")) {
                if (chunk_size < 16) {
                    return Result<AudioSpec>::failure(error(ErrorCode::Format, "WAV fmt chunk is too small"));
                }
                return Result<AudioSpec>::success(AudioSpec{
                    little_u16(bytes, cursor + 8),
                    little_u16(bytes, cursor + 10),
                    little_u16(bytes, cursor + 22),
                    little_u32(bytes, cursor + 12),
                });
            }
            cursor += 8 + chunk_size + (chunk_size & 1);
        }
        return Result<AudioSpec>::failure(error(ErrorCode::Format, "WAV fmt chunk is missing"));
    }

    if (has_mp3_signature(bytes)) {
        return Result<AudioSpec>::success(AudioSpec{0x55, 0, 0, 0});
    }
    return Result<AudioSpec>::failure(error(ErrorCode::Unsupported, "audio format is unsupported"));
}

}  // namespace opennfh::io
