#include "opennfh/presentation/wav_player.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace opennfh::presentation {

namespace {

std::uint8_t byte_at(std::span<const std::byte> bytes, std::size_t offset) {
    return std::to_integer<std::uint8_t>(bytes[offset]);
}

std::uint16_t little_u16(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(byte_at(bytes, offset)) |
           static_cast<std::uint16_t>(byte_at(bytes, offset + 1) << 8);
}

std::uint32_t little_u32(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(byte_at(bytes, offset)) |
           (static_cast<std::uint32_t>(byte_at(bytes, offset + 1)) << 8) |
           (static_cast<std::uint32_t>(byte_at(bytes, offset + 2)) << 16) |
           (static_cast<std::uint32_t>(byte_at(bytes, offset + 3)) << 24);
}

bool has_text(
    std::span<const std::byte> bytes,
    std::size_t offset,
    std::string_view text) {
    if (offset > bytes.size() || text.size() > bytes.size() - offset) {
        return false;
    }
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (byte_at(bytes, offset + index) !=
            static_cast<std::uint8_t>(text[index])) {
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

Result<PcmClip> decode_wav_pcm(std::span<const std::byte> bytes) {
    if (bytes.size() < 12 || !has_text(bytes, 0, "RIFF") ||
        !has_text(bytes, 8, "WAVE")) {
        return Result<PcmClip>::failure(
            error(ErrorCode::Format, "WAV RIFF header is invalid"));
    }
    const auto riff_size = little_u32(bytes, 4);
    if (riff_size > bytes.size() - 8) {
        return Result<PcmClip>::failure(
            error(ErrorCode::Format, "WAV RIFF payload is truncated"));
    }

    bool have_format = false;
    bool have_data = false;
    std::uint16_t format = 0;
    std::uint16_t channels = 0;
    std::uint16_t bits = 0;
    std::uint32_t sample_rate = 0;
    std::uint16_t block_align = 0;
    std::vector<std::byte> samples;

    std::size_t cursor = 12;
    while (cursor < bytes.size()) {
        if (bytes.size() - cursor < 8) {
            return Result<PcmClip>::failure(
                error(ErrorCode::Format, "WAV chunk header is truncated"));
        }
        const auto chunk_size = little_u32(bytes, cursor + 4);
        if (chunk_size > bytes.size() - cursor - 8) {
            return Result<PcmClip>::failure(
                error(ErrorCode::Format, "WAV chunk payload is truncated"));
        }
        const auto payload = cursor + 8;
        if (has_text(bytes, cursor, "fmt ")) {
            if (chunk_size < 16) {
                return Result<PcmClip>::failure(
                    error(ErrorCode::Format, "WAV fmt chunk is too small"));
            }
            format = little_u16(bytes, payload);
            channels = little_u16(bytes, payload + 2);
            sample_rate = little_u32(bytes, payload + 4);
            block_align = little_u16(bytes, payload + 12);
            bits = little_u16(bytes, payload + 14);
            have_format = true;
        } else if (has_text(bytes, cursor, "data")) {
            samples.assign(
                bytes.begin() + static_cast<std::ptrdiff_t>(payload),
                bytes.begin() + static_cast<std::ptrdiff_t>(payload + chunk_size));
            have_data = true;
        }

        cursor = payload + chunk_size;
        if ((chunk_size & 1u) != 0) {
            if (cursor >= bytes.size()) {
                return Result<PcmClip>::failure(
                    error(ErrorCode::Format, "WAV odd chunk padding is missing"));
            }
            ++cursor;
        }
    }

    if (!have_format || !have_data) {
        return Result<PcmClip>::failure(
            error(ErrorCode::Format, "WAV fmt or data chunk is missing"));
    }
    if (format != 1 || (channels != 1 && channels != 2) ||
        (bits != 8 && bits != 16) || sample_rate == 0) {
        return Result<PcmClip>::failure(
            error(ErrorCode::Unsupported, "WAV PCM format is unsupported"));
    }
    const auto expected_align = static_cast<std::uint32_t>(channels) * (bits / 8);
    if (block_align != expected_align ||
        samples.size() % expected_align != 0) {
        return Result<PcmClip>::failure(
            error(ErrorCode::Format, "WAV sample alignment is invalid"));
    }
    return Result<PcmClip>::success(PcmClip{
        static_cast<int>(channels),
        static_cast<int>(sample_rate),
        static_cast<int>(bits),
        std::move(samples),
    });
}

WavPlayer::~WavPlayer() {
    close();
}

Result<bool> WavPlayer::open() {
    if (stream_ != nullptr) {
        return Result<bool>::success(true);
    }
    const SDL_AudioSpec output{
        SDL_AUDIO_S16LE,
        2,
        44100,
    };
    stream_ = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &output, nullptr, nullptr);
    if (stream_ == nullptr) {
        return Result<bool>::failure(error(
            ErrorCode::Io, std::string("SDL audio open failed: ") + SDL_GetError()));
    }
    if (!SDL_ResumeAudioStreamDevice(stream_)) {
        close();
        return Result<bool>::failure(error(
            ErrorCode::Io, std::string("SDL audio resume failed: ") + SDL_GetError()));
    }
    return Result<bool>::success(true);
}

void WavPlayer::close() noexcept {
    if (stream_ != nullptr) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }
}

Result<bool> WavPlayer::play(const PcmClip& clip, int volume) {
    if (stream_ == nullptr) {
        return Result<bool>::failure(
            error(ErrorCode::InvalidArgument, "WAV player is not open"));
    }
    if (clip.sample_rate != 44100 ||
        (clip.channels != 1 && clip.channels != 2) ||
        (clip.bits != 8 && clip.bits != 16)) {
        return Result<bool>::failure(
            error(ErrorCode::Unsupported, "WAV clip cannot be converted to output format"));
    }
    const auto bytes_per_sample = static_cast<std::size_t>(clip.bits / 8);
    const auto frame_bytes = bytes_per_sample * static_cast<std::size_t>(clip.channels);
    if (frame_bytes == 0 || clip.samples.size() % frame_bytes != 0) {
        return Result<bool>::failure(
            error(ErrorCode::Format, "WAV clip has incomplete sample frame"));
    }

    const int clamped_volume = std::clamp(volume, 0, 100);
    const auto frame_count = clip.samples.size() / frame_bytes;
    std::vector<std::int16_t> converted;
    converted.reserve(frame_count * 2);
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        for (int output_channel = 0; output_channel < 2; ++output_channel) {
            const int source_channel = clip.channels == 1 ? 0 : output_channel;
            const auto offset = frame * frame_bytes +
                                static_cast<std::size_t>(source_channel) * bytes_per_sample;
            std::int32_t sample = 0;
            if (clip.bits == 8) {
                sample = (static_cast<std::int32_t>(byte_at(clip.samples, offset)) - 128) * 257;
            } else {
                const auto unsigned_sample =
                    static_cast<std::uint16_t>(byte_at(clip.samples, offset)) |
                    static_cast<std::uint16_t>(byte_at(clip.samples, offset + 1) << 8);
                sample = static_cast<std::int16_t>(unsigned_sample);
            }
            sample = sample * clamped_volume / 100;
            sample = std::clamp(
                sample,
                static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::min()),
                static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::max()));
            converted.push_back(static_cast<std::int16_t>(sample));
        }
    }
    const auto byte_count = converted.size() * sizeof(std::int16_t);
    if (byte_count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return Result<bool>::failure(
            error(ErrorCode::InvalidArgument, "WAV clip is too large for SDL"));
    }
    if (!SDL_PutAudioStreamData(
            stream_, converted.data(), static_cast<int>(byte_count))) {
        return Result<bool>::failure(error(
            ErrorCode::Io, std::string("SDL audio queue failed: ") + SDL_GetError()));
    }
    return Result<bool>::success(true);
}

}  // namespace opennfh::presentation
