#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "opennfh/presentation/wav_player.hpp"

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

void put_text(std::vector<std::byte>& bytes, std::size_t offset, const char* text) {
    for (std::size_t index = 0; text[index] != '\0'; ++index) {
        bytes[offset + index] = static_cast<std::byte>(text[index]);
    }
}

std::vector<std::byte> make_wav(
    std::uint16_t channels,
    std::uint16_t bits,
    const std::vector<std::byte>& samples) {
    const std::uint32_t block_align = channels * (bits / 8);
    const std::uint32_t fmt_size = 16;
    const std::uint32_t junk_size = 1;
    const std::uint32_t data_size = static_cast<std::uint32_t>(samples.size());
    const std::size_t total = 12 + 8 + fmt_size + 8 + junk_size + 1 + 8 + data_size + (data_size & 1);
    std::vector<std::byte> wav(total);
    put_text(wav, 0, "RIFF");
    put_u32(wav, 4, static_cast<std::uint32_t>(total - 8));
    put_text(wav, 8, "WAVE");
    put_text(wav, 12, "fmt ");
    put_u32(wav, 16, fmt_size);
    put_u16(wav, 20, 1);
    put_u16(wav, 22, channels);
    put_u32(wav, 24, 44100);
    put_u32(wav, 28, 44100 * block_align);
    put_u16(wav, 32, static_cast<std::uint16_t>(block_align));
    put_u16(wav, 34, bits);
    std::size_t cursor = 36;
    put_text(wav, cursor, "JUNK");
    put_u32(wav, cursor + 4, junk_size);
    wav[cursor + 8] = static_cast<std::byte>(0x7f);
    cursor += 9 + 1;
    put_text(wav, cursor, "data");
    put_u32(wav, cursor + 4, data_size);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        wav[cursor + 8 + index] = samples[index];
    }
    return wav;
}

}  // namespace

int main() {
    const auto mono8 = make_wav(
        1, 8, {static_cast<std::byte>(0), static_cast<std::byte>(127),
               static_cast<std::byte>(255)});
    const auto decoded8 = opennfh::presentation::decode_wav_pcm(mono8);
    assert(decoded8.has_value());
    assert(decoded8.value().channels == 1);
    assert(decoded8.value().bits == 8);
    assert(decoded8.value().samples.size() == 3);

    const auto stereo16 = make_wav(
        2, 16, {static_cast<std::byte>(0x00), static_cast<std::byte>(0x80),
                static_cast<std::byte>(0xff), static_cast<std::byte>(0x7f)});
    const auto decoded16 = opennfh::presentation::decode_wav_pcm(stereo16);
    assert(decoded16.has_value());
    assert(decoded16.value().channels == 2);
    assert(decoded16.value().bits == 16);
    assert(decoded16.value().samples.size() == 4);

    auto truncated = mono8;
    truncated.resize(truncated.size() - 1);
    assert(!opennfh::presentation::decode_wav_pcm(truncated).has_value());

    opennfh::presentation::WavPlayer player;
    assert(!player.is_open());
    assert(!player.play(decoded8.value(), 100).has_value());
    return 0;
}
