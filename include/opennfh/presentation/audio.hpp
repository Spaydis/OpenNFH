#pragma once

#include <map>
#include <string>
#include <string_view>

#include "opennfh/core/result.hpp"
#include "opennfh/io/audio_catalog.hpp"
#include "opennfh/io/data_root.hpp"

namespace opennfh::presentation {

enum class MusicState {
    Fast,
    Normal,
    Slow,
    Jingle,
};

struct AudioCatalog {
    std::map<std::string, io::AudioSpec> sounds;
    std::map<std::string, int> volumes;
    std::map<MusicState, std::string> music;
};

[[nodiscard]] Result<AudioCatalog> load_audio_catalog(const io::DataRoot& root);

class AudioBackend {
public:
    explicit AudioBackend(AudioCatalog catalog);

    [[nodiscard]] bool has_sound(std::string_view sound_id) const;
    [[nodiscard]] bool play_sound(std::string_view sound_id, float volume);
    void set_music_state(MusicState state) noexcept;

    [[nodiscard]] MusicState current_music_state() const noexcept { return music_state_; }
    [[nodiscard]] std::string_view current_music_track() const noexcept { return music_track_; }
    [[nodiscard]] std::string_view last_sound() const noexcept { return last_sound_; }
    [[nodiscard]] float last_volume() const noexcept { return last_volume_; }
    [[nodiscard]] const AudioCatalog& catalog() const noexcept { return catalog_; }

private:
    AudioCatalog catalog_;
    MusicState music_state_{MusicState::Normal};
    std::string music_track_;
    std::string last_sound_;
    float last_volume_{0.0f};
};

}  // namespace opennfh::presentation
