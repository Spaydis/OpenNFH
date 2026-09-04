#include <fstream>
#include <iostream>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "opennfh/content/loader.hpp"
#include "opennfh/io/data_root.hpp"
#include "opennfh/presentation/audio.hpp"
#include "opennfh/simulation/replay.hpp"

namespace {

bool has_suffix_ci(std::string_view value, std::string_view suffix) {
    if (value.size() < suffix.size()) {
        return false;
    }
    const auto start = value.size() - suffix.size();
    for (std::size_t index = 0; index < suffix.size(); ++index) {
        const auto left = static_cast<unsigned char>(value[start + index]);
        const auto right = static_cast<unsigned char>(suffix[index]);
        if (std::tolower(left) != std::tolower(right)) {
            return false;
        }
    }
    return true;
}

std::size_t count_suffix(
    const opennfh::io::ZipVfs& archive,
    std::string_view suffix) {
    std::size_t count = 0;
    for (const auto& entry : archive.entries()) {
        if (has_suffix_ci(entry.path, suffix)) {
            ++count;
        }
    }
    return count;
}

struct Options {
    std::string data_root;
    std::string level;
    std::string replay;
    bool inspect{false};
    bool headless{false};
    bool show_help{false};
};

void print_help() {
    std::cout << "OpenNFH\n"
              << "  --data-root <path>  use a user-owned data directory\n"
              << "  --inspect           inspect local pack metadata\n"
              << "  --level <id>        load one level by resource id\n"
              << "  --headless          do not create a presentation window\n"
              << "  --replay <path>     validate a deterministic replay\n"
              << "  --help              show this help\n";
}

bool read_value(int& index, int argc, char** argv, std::string& destination) {
    if (index + 1 >= argc) {
        return false;
    }
    destination = argv[++index];
    return true;
}

bool parse_options(int argc, char** argv, Options& options) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--help") {
            options.show_help = true;
            continue;
        }
        if (argument == "--data-root") {
            if (!read_value(index, argc, argv, options.data_root)) {
                std::cerr << "--data-root requires a value\n";
                return false;
            }
            continue;
        }
        if (argument == "--level") {
            if (!read_value(index, argc, argv, options.level)) {
                std::cerr << "--level requires a value\n";
                return false;
            }
            continue;
        }
        if (argument == "--replay") {
            if (!read_value(index, argc, argv, options.replay)) {
                std::cerr << "--replay requires a value\n";
                return false;
            }
            continue;
        }
        if (argument == "--inspect") {
            options.inspect = true;
            continue;
        }
        if (argument == "--headless") {
            options.headless = true;
            continue;
        }
        std::cerr << "unknown option: " << argument << '\n';
        return false;
    }
    return true;
}

opennfh::Result<opennfh::io::DataRoot> open_data_root(const Options& options) {
    if (options.data_root.empty()) {
        opennfh::Error error;
        error.code = opennfh::ErrorCode::InvalidArgument;
        error.message = "--data-root is required for this operation";
        return opennfh::Result<opennfh::io::DataRoot>::failure(std::move(error));
    }
    return opennfh::io::DataRoot::open(options.data_root);
}

int inspect(const Options& options) {
    const auto root_result = open_data_root(options);
    if (!root_result.has_value()) {
        std::cerr << "cannot open data root: " << root_result.error().message << '\n';
        return root_result.error().code == opennfh::ErrorCode::InvalidArgument ? 2 : 1;
    }
    const auto& root = root_result.value();
    const auto audio = opennfh::presentation::load_audio_catalog(root);
    if (!audio.has_value()) {
        std::cerr << "audio metadata error: " << audio.error().message << '\n';
        return 1;
    }
    std::cout << "gamedata_entries=" << root.game_data().entries().size() << '\n'
              << "gamedata_xml=" << count_suffix(root.game_data(), ".xml") << '\n'
              << "gfxdata_entries=" << root.gfx_data().entries().size() << '\n'
              << "gfxdata_tga=" << count_suffix(root.gfx_data(), ".tga") << '\n'
              << "sfxdata_entries=" << root.sfx_data().entries().size() << '\n'
              << "sfxdata_wav=" << count_suffix(root.sfx_data(), ".wav") << '\n'
              << "sfxdata_mp3=" << count_suffix(root.sfx_data(), ".mp3") << '\n'
              << "audio_ids=" << audio.value().sounds.size() << '\n';
    const auto campaign = opennfh::content::load_campaign(root, {});
    if (!campaign.has_value()) {
        std::cerr << "campaign metadata error: " << campaign.error().message << '\n';
        return 1;
    }
    std::size_t levels = 0;
    for (const auto& set : campaign.value().sets) {
        levels += set.levels.size();
    }
    std::cout << "sets=" << campaign.value().sets.size() << '\n'
              << "levels=" << levels << '\n';
    return 0;
}

int load_level(const Options& options) {
    const auto root_result = open_data_root(options);
    if (!root_result.has_value()) {
        std::cerr << "cannot open data root: " << root_result.error().message << '\n';
        return root_result.error().code == opennfh::ErrorCode::InvalidArgument ? 2 : 1;
    }
    opennfh::content::LoadOptions load_options;
    load_options.xml.duplicate_attributes = opennfh::io::DuplicateAttributePolicy::KeepLast;
    const auto level = opennfh::content::load_level(root_result.value(), options.level, load_options);
    if (!level.has_value()) {
        std::cerr << "cannot load level: " << level.error().message << '\n';
        return 1;
    }
    std::size_t room_objects = level.value().root_objects.size();
    for (const auto& room : level.value().rooms) {
        room_objects += room.objects.size();
    }
    std::cout << "level=" << level.value().meta.resource_id << '\n'
              << "rooms=" << level.value().rooms.size() << '\n'
              << "objects=" << room_objects << '\n'
              << "definitions=" << level.value().objects.size() << '\n'
              << "combinations=" << level.value().combinations.size() << '\n';
    return 0;
}

int read_replay(const Options& options) {
    std::ifstream input(options.replay);
    if (!input) {
        std::cerr << "cannot open replay: " << options.replay << '\n';
        return 1;
    }
    const auto replay = opennfh::simulation::read_replay(input);
    if (!replay.has_value()) {
        std::cerr << "cannot read replay: " << replay.error().message << '\n';
        return 1;
    }

    opennfh::simulation::WorldState world;
    if (!options.level.empty()) {
        const auto root_result = open_data_root(options);
        if (!root_result.has_value()) {
            std::cerr << "cannot open data root: " << root_result.error().message << '\n';
            return root_result.error().code == opennfh::ErrorCode::InvalidArgument ? 2 : 1;
        }
        opennfh::content::LoadOptions load_options;
        load_options.xml.duplicate_attributes = opennfh::io::DuplicateAttributePolicy::KeepLast;
        const auto level = opennfh::content::load_level(
            root_result.value(), options.level, load_options);
        if (!level.has_value()) {
            std::cerr << "cannot load level: " << level.error().message << '\n';
            return 1;
        }
        world.level = level.value();
        opennfh::simulation::EntityId next_id = 1;
        for (const auto& room : world.level.rooms) {
            for (const auto& actor : room.actors) {
                world.entities.push_back(opennfh::simulation::EntityState{
                    next_id++, actor.name, room.id, actor.position, actor.layer, true});
            }
        }
    }

    opennfh::simulation::SimulationSnapshot snapshot{
        0, world.entities, world.quotas};
    std::uint64_t final_hash = opennfh::simulation::hash_snapshot(snapshot);
    opennfh::simulation::Tick previous_tick = 0;
    bool has_previous_tick = false;
    for (const auto& event : replay.value().events) {
        if (has_previous_tick && event.tick < previous_tick) {
            std::cerr << "replay events are not ordered by tick\n";
            return 1;
        }
        previous_tick = event.tick;
        has_previous_tick = true;
        snapshot.tick = event.tick;
        final_hash = opennfh::simulation::hash_snapshot(snapshot);
    }

    std::cout << "replay_events=" << replay.value().events.size() << '\n'
              << "mode=" << (options.headless ? "headless" : "asset-free") << '\n'
              << "final_snapshot_hash=" << final_hash << '\n';
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        return 2;
    }
    if (options.show_help || argc == 1) {
        print_help();
        return 0;
    }
    if (options.inspect) {
        return inspect(options);
    }
    if (!options.replay.empty()) {
        return read_replay(options);
    }
    if (!options.level.empty()) {
        return load_level(options);
    }
    print_help();
    return 0;
}
