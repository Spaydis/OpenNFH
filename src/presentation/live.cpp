#include "opennfh/presentation/live.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "opennfh/presentation/assets.hpp"
#include "opennfh/presentation/camera.hpp"
#include "opennfh/presentation/renderer.hpp"
#include "opennfh/presentation/ui.hpp"
#include "opennfh/presentation/wav_player.hpp"
#include "opennfh/simulation/control.hpp"
#include "opennfh/simulation/clock.hpp"
#include "opennfh/simulation/input.hpp"
#include "opennfh/simulation/neighbor_ai.hpp"
#include "opennfh/simulation/scene.hpp"

namespace opennfh::presentation {

namespace {

Error error(ErrorCode code, std::string message) {
    Error result;
    result.code = code;
    result.message = std::move(message);
    return result;
}

InputAction action_for_key(SDL_Keycode key) {
    switch (key) {
    case SDLK_LEFT: return InputAction::ScrollLeft;
    case SDLK_RIGHT: return InputAction::ScrollRight;
    case SDLK_UP: return InputAction::ScrollUp;
    case SDLK_DOWN: return InputAction::ScrollDown;
    case SDLK_HOME: return InputAction::CenterWoody;
    case SDLK_END: return InputAction::FocusNeighbor;
    case SDLK_PAUSE: return InputAction::Pause;
    case SDLK_ESCAPE: return InputAction::Quit;
    default: return InputAction::Unknown;
    }
}

simulation::EntityId controlled_actor(const simulation::WorldState& world) {
    for (const auto& entity : world.entities) {
        if (entity.active && entity.kind == "woody") {
            return entity.id;
        }
    }
    for (const auto& entity : world.entities) {
        if (entity.active) {
            return entity.id;
        }
    }
    return 0;
}

void load_visible_assets(
    const io::DataRoot& root,
    const simulation::WorldState& world,
    const RenderSnapshot& snapshot,
    AssetCache& assets,
    simulation::Tick tick) {
    for (const auto& item : snapshot.items) {
        if (assets.find(item.asset_id) != nullptr) {
            continue;
        }
        const auto image = load_entity_image(root, world, item.entity, tick);
        if (image.has_value()) {
            assets.insert(item.asset_id, image.value());
        }
    }
}

UiSnapshot make_ui_snapshot(
    const io::DataRoot& root,
    std::initializer_list<std::string_view> dialog_ids) {
    UiSnapshot snapshot;
    snapshot.inventory_slots = 0;
    std::vector<std::string_view> loaded_dialogs;
    for (const auto dialog_id : dialog_ids) {
        if (std::find(loaded_dialogs.begin(), loaded_dialogs.end(), dialog_id) !=
            loaded_dialogs.end()) {
            continue;
        }
        loaded_dialogs.push_back(dialog_id);
        const auto definition = load_dialog(root, dialog_id);
        if (!definition.has_value()) {
            continue;
        }
        const auto background_asset_id = "ui:" + definition.value().gfx;
        snapshot.backgrounds.push_back(
            {background_asset_id, definition.value().offset});
        if (snapshot.backgrounds.size() == 1) {
            snapshot.background_asset_id = background_asset_id;
            snapshot.background_position = definition.value().offset;
        }
        for (const auto& control : definition.value().controls) {
            auto rect = control.rect;
            rect.offset.x += definition.value().offset.x;
            rect.offset.y += definition.value().offset.y;
            if (rect.size.x <= 0 || rect.size.y <= 0) {
                if (control.name.starts_with("inv")) {
                    rect.size = {75, 57};
                } else if (control.name == "left" || control.name == "right") {
                    rect.size = {37, 37};
                } else if (!control.images.empty()) {
                    const auto image = load_graphic_image(
                        root, control.images.front().gfx);
                    if (image.has_value()) {
                        rect.size = {
                            static_cast<int>(image.value().info.width),
                            static_cast<int>(image.value().info.height),
                        };
                    }
                }
            }
            snapshot.controls.push_back(UiControlState{
                control.name, rect, false, false, true, control.images});
            if (control.name.starts_with("inv")) {
                ++snapshot.inventory_slots;
            }
        }
    }
    if (snapshot.inventory_slots == 0) {
        snapshot.inventory_slots = 5;
    }
    return snapshot;
}

void load_ui_assets(
    const io::DataRoot& root,
    const simulation::WorldState& world,
    const UiSnapshot& snapshot,
    AssetCache& assets) {
    const auto load = [&](std::string asset_id, std::string_view path) {
        if (path.empty() || assets.find(asset_id) != nullptr) {
            return;
        }
        const auto image = load_graphic_image(root, path);
        if (image.has_value()) {
            assets.insert(std::move(asset_id), image.value());
        }
    };
    for (const auto& background : snapshot.backgrounds) {
        if (background.asset_id.starts_with("ui:")) {
            load(background.asset_id, background.asset_id.substr(3));
        }
    }
    if (snapshot.backgrounds.empty() &&
        snapshot.background_asset_id.starts_with("ui:")) {
        load(snapshot.background_asset_id,
             snapshot.background_asset_id.substr(3));
    }
    for (const auto& control : snapshot.controls) {
        for (const auto& image : control.images) {
            load("ui:" + image.gfx, image.gfx);
        }
    }
    for (const auto& item : snapshot.inventory_items) {
        const auto definition = world.level.objects.find(item);
        if (definition == world.level.objects.end()) {
            continue;
        }
        for (const auto& [state, path] : definition->second.images) {
            load("inventory:" + item + ":" + state, path);
        }
    }
}

std::optional<std::size_t> inventory_slot(std::string_view name) {
    if (!name.starts_with("inv") || name.size() <= 3) {
        return std::nullopt;
    }
    std::size_t value = 0;
    const auto parsed = std::from_chars(
        name.data() + 3, name.data() + name.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != name.data() + name.size()) {
        return std::nullopt;
    }
    return value;
}

}  // namespace

Result<LiveOptions> validate_live_options(LiveOptions options) {
    if (options.window_width <= 0 || options.window_height <= 0) {
        return Result<LiveOptions>::failure(error(
            ErrorCode::InvalidArgument, "live window dimensions must be positive"));
    }
    if (options.dialog_id.empty()) {
        return Result<LiveOptions>::failure(error(
            ErrorCode::InvalidArgument, "live dialog ID is empty"));
    }
    if (options.logic_fps < 1 || options.logic_fps > 60) {
        return Result<LiveOptions>::failure(error(
            ErrorCode::InvalidArgument, "live logic FPS must be between 1 and 60"));
    }
    return Result<LiveOptions>::success(std::move(options));
}

Result<bool> validate_play_request(
    bool headless,
    std::string_view data_root,
    std::string_view level) {
    if (headless) {
        return Result<bool>::failure(error(
            ErrorCode::InvalidArgument, "--headless cannot be combined with --play"));
    }
    if (data_root.empty()) {
        return Result<bool>::failure(error(
            ErrorCode::InvalidArgument, "--play requires --data-root"));
    }
    if (level.empty()) {
        return Result<bool>::failure(error(
            ErrorCode::InvalidArgument, "--play requires --level"));
    }
    return Result<bool>::success(true);
}

Result<int> run_level(
    const io::DataRoot& root,
    content::LevelDefinition level,
    LiveOptions options) {
    const auto valid_options = validate_live_options(std::move(options));
    if (!valid_options.has_value()) {
        return Result<int>::failure(valid_options.error());
    }
    options = valid_options.value();
    if (level.meta.resource_id.empty()) {
        return Result<int>::failure(error(
            ErrorCode::Missing, "live level has no resource ID"));
    }

    auto world = simulation::make_world(std::move(level));
    const auto actor = controlled_actor(world);
    if (actor == 0) {
        return Result<int>::failure(error(
            ErrorCode::Missing, "live level has no active actor"));
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return Result<int>::failure(error(
            ErrorCode::Io, std::string("SDL video init failed: ") + SDL_GetError()));
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer(
            "OpenNFH", options.window_width, options.window_height,
            SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        const std::string message = std::string("SDL window creation failed: ") + SDL_GetError();
        SDL_Quit();
        return Result<int>::failure(error(ErrorCode::Io, message));
    }

    const bool audio_subsystem = SDL_InitSubSystem(SDL_INIT_AUDIO);
    WavPlayer wav_player;
    if (audio_subsystem) {
        const auto audio = wav_player.open();
        if (!audio.has_value()) {
            std::cerr << "WAV output unavailable: "
                      << audio.error().message << '\n';
        }
    }

    AssetCache assets;
    auto ui = make_ui_snapshot(
        root, {"menucentertop", "menuleft", "menu_bubble",
               "menuleft_bar", "menuright", options.dialog_id});
    sync_inventory(ui, world);
    simulation::ControlState control;
    control.actor = actor;
    auto camera = make_camera(world, {800, 600}, actor);
    std::size_t noise_cursor = 0;
    simulation::Tick tick = 0;
    bool running = true;
    bool paused = false;
    simulation::LogicClock logic_clock;
    simulation::set_logic_fps(logic_clock, options.logic_fps);
    std::uint64_t previous_ms = SDL_GetTicks();

    while (running) {
        int window_width = options.window_width;
        int window_height = options.window_height;
        SDL_GetWindowSize(window, &window_width, &window_height);
        auto snapshot = make_render_snapshot(world, tick, camera.offset, camera.viewport);
        load_visible_assets(root, world, snapshot, assets, tick);
        sync_inventory(ui, world);
        load_ui_assets(root, world, ui, assets);
        const auto transform = make_viewport(ViewportConfig{
            snapshot.logical_size, window_width, window_height, options.integer_scale});
        const auto ui_transform = make_viewport(ViewportConfig{
            {1280, 720}, window_width, window_height, false});
        const auto regions = make_hit_regions(world, snapshot, assets, tick);

        for (auto& control_state : ui.controls) {
            control_state.hovered = false;
            control_state.pressed = false;
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.down && !event.key.repeat) {
                const auto action = action_for_key(event.key.key);
                if (action == InputAction::ScrollLeft) {
                    scroll_camera(camera, {-32, 0});
                } else if (action == InputAction::ScrollRight) {
                    scroll_camera(camera, {32, 0});
                } else if (action == InputAction::ScrollUp) {
                    scroll_camera(camera, {0, -32});
                } else if (action == InputAction::ScrollDown) {
                    scroll_camera(camera, {0, 32});
                } else if (action == InputAction::CenterWoody) {
                    camera.focus = actor;
                    camera.follow_focus = true;
                    update_camera(camera, world);
                } else if (action == InputAction::FocusNeighbor) {
                    for (const auto& entity : world.entities) {
                        if (entity.active && entity.kind == "neighbor") {
                            camera.focus = entity.id;
                            camera.follow_focus = true;
                            update_camera(camera, world);
                            break;
                        }
                    }
                } else if (action == InputAction::Quit) {
                    running = false;
                } else if (action == InputAction::Pause) {
                    paused = !paused;
                }
                continue;
            }
            if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN ||
                !event.button.down ||
                (event.button.button != SDL_BUTTON_LEFT &&
                 event.button.button != SDL_BUTTON_RIGHT)) {
                continue;
            }
            const auto ui_cursor = ui_transform.to_logical({
                static_cast<int>(event.button.x),
                static_cast<int>(event.button.y),
            });
            const bool sneak = event.button.button == SDL_BUTTON_RIGHT;
            if (!sneak) {
                const auto ui_target = hit_ui_button(ui, ui_cursor);
                if (ui_target.has_value()) {
                    auto& ui_control = ui.controls[ui_target.value()];
                    ui_control.hovered = true;
                    ui_control.pressed = true;
                    if (const auto slot = inventory_slot(ui_control.name);
                        slot.has_value()) {
                        if (slot.value() < ui.inventory_items.size()) {
                            ui.selected_slot = slot.value();
                            control.selected_item =
                                ui.inventory_items[slot.value()];
                        }
                    } else if (ui_control.name == "left") {
                        if (ui.inventory_start >= ui.inventory_slots) {
                            ui.inventory_start -= ui.inventory_slots;
                        } else {
                            ui.inventory_start = 0;
                        }
                        ui.selected_slot.reset();
                        sync_inventory(ui, world);
                    } else if (ui_control.name == "right") {
                        ui.inventory_start += ui.inventory_slots;
                        ui.selected_slot.reset();
                        sync_inventory(ui, world);
                    }
                    continue;
                }
            }
            const auto cursor = transform.to_logical({
                static_cast<int>(event.button.x),
                static_cast<int>(event.button.y),
            });
            if (cursor.x < 0 || cursor.y < 0 ||
                cursor.x >= snapshot.logical_size.x ||
                cursor.y >= snapshot.logical_size.y) {
                continue;
            }
            const auto target = simulation::hit_test(regions, cursor);
            const auto level_cursor = Vec2i{
                cursor.x + camera.offset.x,
                cursor.y + camera.offset.y,
            };
            const auto handled = simulation::handle_click(
                world, control, actor, level_cursor,
                sneak ? 0 : (target.has_value() ? target.value() : 0),
                sneak ? simulation::MovementMode::Sneak
                      : simulation::MovementMode::Walk);
            if (!handled.has_value() && target.has_value()) {
                std::cerr << "click rejected: " << handled.error().message << '\n';
            }
        }

        const auto now_ms = SDL_GetTicks();
        const auto elapsed_ms = std::min<std::uint64_t>(now_ms - previous_ms, 250);
        previous_ms = now_ms;
        const auto logic_ticks = simulation::consume_logic_ticks(logic_clock, elapsed_ms);
        for (int step = 0; step < logic_ticks; ++step) {
            ++tick;
            if (paused) {
                continue;
            }
            simulation::update_control(world, control, tick);
            while (noise_cursor < world.emitted_noise.size()) {
                simulation::dispatch_noise(
                    world, world.emitted_noise[noise_cursor], tick);
                ++noise_cursor;
            }
            simulation::update_neighbor_ai(world, tick);
            update_camera(camera, world);
        }

        snapshot = make_render_snapshot(world, tick, camera.offset, camera.viewport);
        load_visible_assets(root, world, snapshot, assets, tick);
        render_scene(renderer, snapshot, assets, transform);
        sync_inventory(ui, world);
        load_ui_assets(root, world, ui, assets);
        draw_ui(renderer, ui, assets, ui_transform);
        SDL_RenderPresent(renderer);
        SDL_Delay(1);
    }

    wav_player.close();
    if (audio_subsystem) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    assets.release_renderer(renderer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return Result<int>::success(0);
}

}  // namespace opennfh::presentation
