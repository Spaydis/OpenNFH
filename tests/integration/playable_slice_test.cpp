#define _CRT_SECURE_NO_WARNINGS

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "opennfh/content/loader.hpp"
#include "opennfh/io/data_root.hpp"
#include "opennfh/presentation/ui.hpp"
#include "opennfh/simulation/replay.hpp"
#include "opennfh/simulation/scene.hpp"

int main() {
    const char* path = std::getenv("OPENNFH_DATA_ROOT");
    if (path == nullptr || *path == '\0') {
        std::cout << "SKIPPED: OPENNFH_DATA_ROOT is not configured\n";
        return 0;
    }

    const auto root = opennfh::io::DataRoot::open(path);
    assert(root.has_value());

    opennfh::content::LoadOptions options;
    options.xml.duplicate_attributes =
        opennfh::io::DuplicateAttributePolicy::KeepLast;
    const auto level = opennfh::content::load_level(
        root.value(), "level_mail", options);
    assert(level.has_value());

    auto world = opennfh::simulation::make_world(level.value());
    assert(!world.entities.empty());
    const auto dialog = opennfh::presentation::load_dialog(root.value(), "menu");
    assert(dialog.has_value());
    assert(!dialog.value().controls.empty());

    opennfh::simulation::EntityId actor = 0;
    for (const auto& entity : world.entities) {
        if (entity.active && entity.kind == "woody") {
            actor = entity.id;
            break;
        }
    }
    assert(actor != 0);
    const opennfh::simulation::Replay trace{
        1,
        {opennfh::simulation::InputEvent{
            0, opennfh::simulation::InputAction::Quit, {0, 0}, {}, {}}},
    };
    const auto replay = opennfh::simulation::run_replay(
        world, trace, {actor, 0, true});
    assert(replay.has_value());
    assert(replay.value().stopped_by_quit);

    std::cout << "rooms=" << level.value().rooms.size()
              << " entities=" << world.entities.size()
              << " dialog_controls=" << dialog.value().controls.size()
              << " replay_hash=" << replay.value().snapshot_hash << '\n';
    return 0;
}
