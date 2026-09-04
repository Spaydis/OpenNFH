#include "opennfh/io/audio_catalog.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace opennfh::io {

namespace {

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

    if ((bytes.size() >= 3 && has_text(bytes, 0, "ID3")) ||
        (bytes.size() >= 2 && std::to_integer<unsigned char>(bytes[0]) == 0xff &&
         (std::to_integer<unsigned char>(bytes[1]) & 0xe0) == 0xe0)) {
        return Result<AudioSpec>::success(AudioSpec{0x55, 0, 0, 0});
    }
    return Result<AudioSpec>::failure(error(ErrorCode::Unsupported, "audio format is unsupported"));
}

}  // namespace opennfh::io
