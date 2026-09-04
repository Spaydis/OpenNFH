#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <SDL3/SDL.h>

#include "opennfh/core/result.hpp"

namespace opennfh::presentation {

struct PcmClip {
    int channels{0};
    int sample_rate{0};
    int bits{0};
    std::vector<std::byte> samples;
};

[[nodiscard]] Result<PcmClip> decode_wav_pcm(
    std::span<const std::byte> bytes);

class WavPlayer {
public:
    WavPlayer() = default;
    ~WavPlayer();

    WavPlayer(const WavPlayer&) = delete;
    WavPlayer& operator=(const WavPlayer&) = delete;
    WavPlayer(WavPlayer&&) = delete;
    WavPlayer& operator=(WavPlayer&&) = delete;

    [[nodiscard]] Result<bool> open();
    void close() noexcept;
    [[nodiscard]] Result<bool> play(const PcmClip& clip, int volume);
    [[nodiscard]] bool is_open() const noexcept { return stream_ != nullptr; }

private:
    SDL_AudioStream* stream_{nullptr};
};

}  // namespace opennfh::presentation
