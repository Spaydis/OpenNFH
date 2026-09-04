#include <cassert>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <cstddef>
#include <filesystem>
#include <string_view>
#include <vector>

#include <zip.h>

#include "opennfh/io/data_root.hpp"
#include "opennfh/presentation/audio.hpp"

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

std::vector<std::byte> make_wav() {
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
    return wav;
}

std::vector<std::byte> make_padded_mp3() {
    std::vector<std::byte> mp3(421);
    mp3[417] = static_cast<std::byte>(0xff);
    mp3[418] = static_cast<std::byte>(0xfb);
    mp3[419] = static_cast<std::byte>(0x90);
    mp3[420] = static_cast<std::byte>(0x60);
    return mp3;
}

void add_entry(zip_t* archive, const char* path, const std::vector<std::byte>& bytes) {
    zip_source_t* source = zip_source_buffer(archive, bytes.data(), bytes.size(), 0);
    assert(source != nullptr);
    assert(zip_file_add(archive, path, source, ZIP_FL_ENC_UTF_8) >= 0);
}

class AudioDataFixture {
public:
    AudioDataFixture() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() /
                ("opennfh-audio-fixture-" + std::to_string(stamp));
        std::filesystem::create_directories(root_);

        create_archive(root_ / "gfxdata.bnd", {{"placeholder.bin", std::vector<std::byte>{static_cast<std::byte>(0)}}});
        create_archive(root_ / "gamedata.bnd", {
            {"generic/sfxdata.xml", std::vector<std::byte>{
                static_cast<std::byte>('<'), static_cast<std::byte>('s'),
                static_cast<std::byte>('f'), static_cast<std::byte>('x'),
                static_cast<std::byte>('d'), static_cast<std::byte>('a'),
                static_cast<std::byte>('t'), static_cast<std::byte>('a'),
                static_cast<std::byte>('>'), static_cast<std::byte>('<'),
                static_cast<std::byte>('s'), static_cast<std::byte>('f'),
                static_cast<std::byte>('x'), static_cast<std::byte>(' '),
                static_cast<std::byte>('f'), static_cast<std::byte>('i'),
                static_cast<std::byte>('l'), static_cast<std::byte>('e'),
                static_cast<std::byte>('='), static_cast<std::byte>('"'),
                static_cast<std::byte>('s'), static_cast<std::byte>('f'),
                static_cast<std::byte>('x'), static_cast<std::byte>('/'),
                static_cast<std::byte>('c'), static_cast<std::byte>('l'),
                static_cast<std::byte>('i'), static_cast<std::byte>('c'),
                static_cast<std::byte>('k'), static_cast<std::byte>('.'),
                static_cast<std::byte>('w'), static_cast<std::byte>('a'),
                static_cast<std::byte>('v'), static_cast<std::byte>('"'),
                static_cast<std::byte>(' '), static_cast<std::byte>('v'),
                static_cast<std::byte>('o'), static_cast<std::byte>('l'),
                static_cast<std::byte>('u'), static_cast<std::byte>('m'),
                static_cast<std::byte>('e'), static_cast<std::byte>('='),
                static_cast<std::byte>('"'), static_cast<std::byte>('4'),
                static_cast<std::byte>('2'), static_cast<std::byte>('"'),
                static_cast<std::byte>('/'), static_cast<std::byte>('>'),
                static_cast<std::byte>('<'), static_cast<std::byte>('/'),
                static_cast<std::byte>('s'), static_cast<std::byte>('f'),
                static_cast<std::byte>('x'), static_cast<std::byte>('d'),
                static_cast<std::byte>('a'), static_cast<std::byte>('t'),
                static_cast<std::byte>('a'), static_cast<std::byte>('>'),
            }},
        });
        create_archive(root_ / "sfxdata.bnd", {
            {"sfx/click.wav", make_wav()},
            {"music/game_fast.mp3", make_padded_mp3()},
        });
    }

    ~AudioDataFixture() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept {
        return root_;
    }

private:
    struct Entry {
        const char* path;
        std::vector<std::byte> bytes;
    };

    static void create_archive(
        const std::filesystem::path& path,
        std::initializer_list<Entry> entries) {
        int error = 0;
        zip_t* archive = zip_open(path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
        assert(archive != nullptr);
        for (const auto& entry : entries) {
            add_entry(archive, entry.path, entry.bytes);
        }
        assert(zip_close(archive) == 0);
    }

    std::filesystem::path root_;
};

}  // namespace

int main() {
    AudioDataFixture fixture;
    const auto root = opennfh::io::DataRoot::open(fixture.root());
    assert(root.has_value());
    const auto loaded = opennfh::presentation::load_audio_catalog(root.value());
    assert(loaded.has_value());
    assert(loaded.value().sounds.size() == 2);
    assert(loaded.value().volumes.at("sfx/click.wav") == 42);
    assert(loaded.value().music.at(opennfh::presentation::MusicState::Fast) ==
           "music/game_fast.mp3");

    using opennfh::io::AudioSpec;
    using opennfh::presentation::AudioBackend;
    using opennfh::presentation::AudioCatalog;
    using opennfh::presentation::MusicState;

    AudioCatalog catalog;
    catalog.sounds.emplace("door", AudioSpec{1, 1, 16, 44100});
    catalog.volumes.emplace("door", 72);
    catalog.music.emplace(MusicState::Normal, "music_normal");
    catalog.music.emplace(MusicState::Fast, "music_fast");

    AudioBackend backend(catalog);
    assert(backend.has_sound("door"));
    assert(!backend.has_sound("missing"));
    assert(backend.play_sound("door", 0.25f));
    assert(backend.last_sound() == "door");
    assert(backend.last_volume() == 0.25f);
    assert(!backend.play_sound("missing", 1.0f));

    backend.set_music_state(MusicState::Fast);
    assert(backend.current_music_state() == MusicState::Fast);
    assert(backend.current_music_track() == "music_fast");
    return 0;
}
