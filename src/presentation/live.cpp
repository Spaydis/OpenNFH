#include "opennfh/presentation/live.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "opennfh/presentation/assets.hpp"
#include "opennfh/presentation/renderer.hpp"
#include "opennfh/presentation/ui.hpp"
#include "opennfh/simulation/actions.hpp"
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
    AssetCache& assets) {
    for (const auto& item : snapshot.items) {
        if (assets.find(item.asset_id) != nullptr) {
            continue;
        }
        const auto image = load_entity_image(root, world, item.entity);
        if (image.has_value()) {
            assets.insert(item.asset_id, image.value());
        }
    }
}

std::vector<simulation::HitRegion> hit_regions(
    const RenderSnapshot& snapshot,
    const AssetCache& assets) {
    std::vector<simulation::HitRegion> regions;
    for (const auto& item : snapshot.items) {
        const auto* image = assets.find(item.asset_id);
        if (image == nullptr) {
            continue;
        }
        regions.push_back(simulation::HitRegion{
            item.entity,
            item.position,
            {static_cast<int>(image->info.width), static_cast<int>(image->info.height)},
            item.layer,
            item.y_order,
            item.source_order,
            true,
        });
    }
    return regions;
}

UiSnapshot make_ui_snapshot(
    const io::DataRoot& root,
    std::string_view dialog_id) {
    UiSnapshot snapshot;
    const auto definition = load_dialog(root, dialog_id);
    if (!definition.has_value()) {
        return snapshot;
    }
    for (const auto& control : definition.value().controls) {
        snapshot.controls.push_back(UiControlState{
            control.name, control.rect, false, false, true});
    }
    return snapshot;
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

    AssetCache assets;
    const auto ui = make_ui_snapshot(root, options.dialog_id);
    std::map<simulation::EntityId, simulation::ActionTransaction> active_actions;
    std::size_t noise_cursor = 0;
    simulation::Tick tick = 0;
    bool running = true;
    bool paused = false;
    std::uint64_t previous_ms = SDL_GetTicks();
    std::uint64_t accumulator_ms = 0;

    while (running) {
        int window_width = options.window_width;
        int window_height = options.window_height;
        SDL_GetWindowSize(window, &window_width, &window_height);
        const auto snapshot = make_render_snapshot(world);
        load_visible_assets(root, world, snapshot, assets);
        const auto transform = make_viewport(ViewportConfig{
            snapshot.logical_size, window_width, window_height, options.integer_scale});
        const auto regions = hit_regions(snapshot, assets);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.down && !event.key.repeat) {
                const auto action = action_for_key(event.key.key);
                if (action == InputAction::Quit) {
                    running = false;
                } else if (action == InputAction::Pause) {
                    paused = !paused;
                }
                continue;
            }
            if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN ||
                !event.button.down || event.button.button != 1) {
                continue;
            }
            const auto target = simulation::hit_test(
                regions, transform.to_logical({
                    static_cast<int>(event.button.x),
                    static_cast<int>(event.button.y),
                }));
            if (!target.has_value()) {
                continue;
            }
            const auto request = simulation::action_request_for(world, actor, target.value());
            if (!request.has_value()) {
                continue;
            }
            const auto started = simulation::begin_action(world, request.value(), tick);
            if (started.has_value()) {
                active_actions.emplace(started.value().actor, started.value());
            }
        }

        const auto now_ms = SDL_GetTicks();
        const auto elapsed_ms = std::min<std::uint64_t>(now_ms - previous_ms, 250);
        previous_ms = now_ms;
        accumulator_ms += elapsed_ms;
        while (accumulator_ms >= 16) {
            accumulator_ms -= 16;
            ++tick;
            if (paused) {
                continue;
            }
            for (auto action = active_actions.begin(); action != active_actions.end();) {
                simulation::advance_action(world, action->second, tick);
                if (action->second.committed) {
                    action = active_actions.erase(action);
                } else {
                    ++action;
                }
            }
            while (noise_cursor < world.emitted_noise.size()) {
                simulation::dispatch_noise(
                    world, world.emitted_noise[noise_cursor], tick);
                ++noise_cursor;
            }
            simulation::update_neighbor_ai(world, tick);
        }

        render_scene(renderer, snapshot, assets, transform);
        draw_ui(renderer, ui, transform);
        SDL_RenderPresent(renderer);
        SDL_Delay(1);
    }

    assets.release_renderer(renderer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return Result<int>::success(0);
}

}  // namespace opennfh::presentation
