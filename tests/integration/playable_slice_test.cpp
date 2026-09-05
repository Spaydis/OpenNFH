#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>

#include "opennfh/content/loader.hpp"
#include "opennfh/io/data_root.hpp"
#include "opennfh/presentation/assets.hpp"
#include "opennfh/presentation/camera.hpp"
#include "opennfh/presentation/ui.hpp"
#include "opennfh/simulation/control.hpp"
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
    const auto woody_definition = world.level.objects.find("woody");
    assert(woody_definition != world.level.objects.end());
    assert(std::any_of(
        woody_definition->second.speeds.begin(), woody_definition->second.speeds.end(),
        [](const auto& speed) { return speed.name == "mg0" && speed.speed == 3; }));
    const auto render = opennfh::presentation::make_render_snapshot(world);
    assert(!render.items.empty());
    bool woody_rendered = false;
    bool door_rendered = false;
    std::size_t decoded = 0;
    for (const auto& item : render.items) {
        const auto image = opennfh::presentation::load_entity_image(
            root.value(), world, item.entity);
        if (!image.has_value()) continue;
        ++decoded;
        for (const auto& entity : world.entities) {
            if (entity.id != item.entity) continue;
            if (entity.kind == "woody") woody_rendered = true;
            const auto object = world.level.objects.find(entity.kind);
            if (object != world.level.objects.end() && object->second.kind == "door") {
                door_rendered = true;
            }
            break;
        }
    }
    assert(decoded > 0);
    assert(woody_rendered);
    assert(door_rendered);

    auto door_world = opennfh::simulation::make_world(level.value());
    opennfh::simulation::EntityId door_actor = 0;
    opennfh::simulation::EntityId fro_door = 0;
    for (const auto& entity : door_world.entities) {
        if (entity.active && entity.kind == "woody") door_actor = entity.id;
        if (entity.active && entity.kind == "fro/anc") fro_door = entity.id;
    }
    assert(door_actor != 0);
    assert(fro_door != 0);
    opennfh::simulation::ControlState door_control;
    const auto door_click = opennfh::simulation::handle_click(
        door_world, door_control, door_actor, {}, fro_door);
    assert(door_click.has_value());
    for (opennfh::simulation::Tick tick = 1; tick <= 1000; ++tick) {
        opennfh::simulation::update_control(door_world, door_control, tick);
    }
    assert(door_world.current_room(door_actor) == "anc2");
    assert(!door_control.door_traversal.has_value());
    const auto dialog = opennfh::presentation::load_dialog(root.value(), "menu");
    assert(dialog.has_value());
    assert(!dialog.value().controls.empty());
    assert(dialog.value().gfx == "gui/ingame/interface_m.tga");
    for (const auto dialog_id : {
             "menucentertop", "menuleft", "menuright",
             "menu_bubble", "menuleft_bar"}) {
        const auto hud_dialog =
            opennfh::presentation::load_dialog(root.value(), dialog_id);
        assert(hud_dialog.has_value());
        assert(!hud_dialog.value().gfx.empty());
    }
    bool has_inventory_images = false;
    for (const auto& [name, definition] : world.level.objects) {
        if (definition.kind == "inventar" &&
            definition.images.contains("std")) {
            has_inventory_images = true;
            break;
        }
    }
    assert(has_inventory_images);

    opennfh::simulation::EntityId actor = 0;
    for (const auto& entity : world.entities) {
        if (entity.active && entity.kind == "woody") {
            actor = entity.id;
            break;
        }
    }
    assert(actor != 0);
    const auto camera = opennfh::presentation::make_camera(world, {800, 600}, actor);
    const auto camera_render = opennfh::presentation::make_render_snapshot(
        world, 0, camera.offset, camera.viewport);
    assert(camera_render.logical_size.x == 800);
    assert(camera_render.logical_size.y == 600);
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
