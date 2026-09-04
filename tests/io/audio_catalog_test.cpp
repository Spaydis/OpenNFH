#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "opennfh/io/audio_catalog.hpp"

namespace {

void put_u16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xff);
    bytes[offset + 1] = static_cast<std::byte>(value >> 8);
}

void put_u32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xff);
    bytes[offset + 1] = static_cast<std::byte>((value >> 8) & 0xff);
    bytes[offset + 2] = static_cast<std::byte>((value >> 16) & 0xff);
    bytes[offset + 3] = static_cast<std::byte>(value >> 24);
}

void put_text(std::vector<std::byte>& bytes, std::size_t offset, std::string_view text) {
    for (std::size_t index = 0; index < text.size(); ++index) {
        bytes[offset + index] = static_cast<std::byte>(text[index]);
    }
}

}  // namespace

int main() {
    std::vector<std::byte> wav(44);
    put_text(wav, 0, "RIFF");
    put_u32(wav, 4, 36);
    put_text(wav, 8, "WAVE");
    put_text(wav, 12, "fmt ");
    put_u32(wav, 16, 16);
    put_u16(wav, 20, 1);
    put_u16(wav, 22, 1);
    put_u32(wav, 24, 44100);
    put_u32(wav, 28, 88200);
    put_u16(wav, 32, 2);
    put_u16(wav, 34, 16);
    put_text(wav, 36, "data");
    put_u32(wav, 40, 0);

    const auto wav_result = opennfh::io::inspect_audio(wav);
    assert(wav_result.has_value());
    assert(wav_result.value().format == 1);
    assert(wav_result.value().channels == 1);
    assert(wav_result.value().sample_rate == 44100);
    assert(wav_result.value().bits == 16);

    std::vector<std::byte> mp3(16);
    put_text(mp3, 0, "ID3");
    mp3[3] = static_cast<std::byte>(3);
    mp3[10] = static_cast<std::byte>(0xff);
    mp3[11] = static_cast<std::byte>(0xfb);
    mp3[12] = static_cast<std::byte>(0x90);
    mp3[13] = static_cast<std::byte>(0x60);
    const auto mp3_result = opennfh::io::inspect_audio(mp3);
    assert(mp3_result.has_value());
    assert(mp3_result.value().format == 0x55);

    std::vector<std::byte> id3_only(10);
    put_text(id3_only, 0, "ID3");
    id3_only[3] = static_cast<std::byte>(3);
    const auto id3_only_result = opennfh::io::inspect_audio(id3_only);
    assert(!id3_only_result.has_value());

    std::vector<std::byte> padded_mp3(421);
    padded_mp3[417] = static_cast<std::byte>(0xff);
    padded_mp3[418] = static_cast<std::byte>(0xfb);
    padded_mp3[419] = static_cast<std::byte>(0x90);
    padded_mp3[420] = static_cast<std::byte>(0x60);
    const auto padded_result = opennfh::io::inspect_audio(padded_mp3);
    assert(padded_result.has_value());
    assert(padded_result.value().format == 0x55);

    std::vector<std::byte> invalid_frame = {
        static_cast<std::byte>(0xff), static_cast<std::byte>(0xe0),
        static_cast<std::byte>(0x00), static_cast<std::byte>(0x00),
    };
    const auto invalid_result = opennfh::io::inspect_audio(invalid_frame);
    assert(!invalid_result.has_value());
    return 0;
}
