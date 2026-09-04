#include "opennfh/presentation/audio.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

#include "opennfh/io/text_codec.hpp"
#include "opennfh/io/xml_fragments.hpp"

namespace opennfh::presentation {

namespace {

bool has_suffix(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() &&
           value.substr(value.size() - suffix.size()) == suffix;
}

std::string lower_copy(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const unsigned char character : value) {
        result.push_back(static_cast<char>(std::tolower(character)));
    }
    return result;
}

std::string without_extension(std::string_view path) {
    const auto separator = path.find_last_of("/\\");
    const auto extension = path.find_last_of('.');
    if (extension == std::string_view::npos ||
        (separator != std::string_view::npos && extension < separator)) {
        return std::string(path);
    }
    return std::string(path.substr(0, extension));
}

void add_sound(AudioCatalog& catalog, std::string id, const io::AudioSpec& spec) {
    if (id.empty()) {
        return;
    }
    catalog.sounds.emplace(id, spec);
    catalog.volumes.emplace(std::move(id), 100);
}

void add_music_candidate(AudioCatalog& catalog, MusicState state, std::string_view path) {
    if (!catalog.music.contains(state)) {
        catalog.music.emplace(state, path);
    }
}

std::string_view attribute(const io::XmlNode& node, std::string_view name) {
    for (const auto& [key, value] : node.attributes) {
        if (key == name) {
            return value;
        }
    }
    return {};
}

bool parse_volume(std::string_view text, int& result) {
    if (text.empty()) {
        return false;
    }
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

Result<bool> merge_sfx_metadata(AudioCatalog& catalog, const io::DataRoot& root) {
    for (const auto& entry : root.game_data().entries()) {
        const auto lower_path = lower_copy(entry.path);
        if (!has_suffix(lower_path, "sfxdata.xml")) {
            continue;
        }
        const auto bytes = root.game_data().read(entry.path);
        if (!bytes.has_value()) {
            return Result<bool>::failure(bytes.error());
        }
        const auto text = io::decode_xml_bytes(bytes.value());
        if (!text.has_value()) {
            return Result<bool>::failure(text.error());
        }
        const std::string xml = text.value();
        const auto document = io::parse_xml_fragments(entry.path, xml, {});
        if (!document.has_value()) {
            return Result<bool>::failure(document.error());
        }
        for (const auto& fragment : document.value().roots) {
            if (fragment.name != "sfxdata") {
                continue;
            }
            for (const auto& child : fragment.children) {
                if (child.name != "sfx") {
                    continue;
                }
                const auto file = attribute(child, "file");
                int volume = 100;
                if (!file.empty() && parse_volume(attribute(child, "volume"), volume)) {
                    catalog.volumes[std::string(file)] = std::clamp(volume, 0, 100);
                }
            }
        }
    }
    return Result<bool>::success(true);
}

}  // namespace

Result<AudioCatalog> load_audio_catalog(const io::DataRoot& root) {
    AudioCatalog catalog;
    for (const auto& entry : root.sfx_data().entries()) {
        const auto lower_path = lower_copy(entry.path);
        if (!has_suffix(lower_path, ".wav") && !has_suffix(lower_path, ".mp3")) {
            continue;
        }
        const auto bytes = root.sfx_data().read(entry.path);
        if (!bytes.has_value()) {
            return Result<AudioCatalog>::failure(bytes.error());
        }
        const auto spec = io::inspect_audio(bytes.value());
        if (!spec.has_value()) {
            auto error = spec.error();
            error.source = entry.path;
            return Result<AudioCatalog>::failure(std::move(error));
        }

        add_sound(catalog, entry.path, spec.value());

        const auto filename_start = lower_path.find_last_of("/\\");
        const auto filename = lower_path.substr(
            filename_start == std::string_view::npos ? 0 : filename_start + 1);
        if (filename.find("fast") != std::string_view::npos) {
            add_music_candidate(catalog, MusicState::Fast, entry.path);
        } else if (filename.find("slow") != std::string_view::npos) {
            add_music_candidate(catalog, MusicState::Slow, entry.path);
        } else if (filename.find("jingle") != std::string_view::npos ||
                   filename.find("win") != std::string_view::npos) {
            add_music_candidate(catalog, MusicState::Jingle, entry.path);
        } else if (filename.find("normal") != std::string_view::npos ||
                   filename.find("music") != std::string_view::npos) {
            add_music_candidate(catalog, MusicState::Normal, entry.path);
        }
    }
    const auto metadata = merge_sfx_metadata(catalog, root);
    if (!metadata.has_value()) {
        return Result<AudioCatalog>::failure(metadata.error());
    }
    return Result<AudioCatalog>::success(std::move(catalog));
}

AudioBackend::AudioBackend(AudioCatalog catalog)
    : catalog_(std::move(catalog)) {
    const auto music = catalog_.music.find(music_state_);
    if (music != catalog_.music.end()) {
        music_track_ = music->second;
    }
}

bool AudioBackend::has_sound(std::string_view sound_id) const {
    if (catalog_.sounds.contains(std::string(sound_id))) {
        return true;
    }
    for (const auto& [id, spec] : catalog_.sounds) {
        (void)spec;
        if (without_extension(id) == sound_id) {
            return true;
        }
    }
    return false;
}

bool AudioBackend::play_sound(std::string_view sound_id, float volume) {
    if (!has_sound(sound_id)) {
        return false;
    }
    last_sound_ = sound_id;
    last_volume_ = std::clamp(volume, 0.0f, 1.0f);
    return true;
}

void AudioBackend::set_music_state(MusicState state) noexcept {
    music_state_ = state;
    const auto music = catalog_.music.find(state);
    music_track_ = music == catalog_.music.end() ? std::string{} : music->second;
}

}  // namespace opennfh::presentation
