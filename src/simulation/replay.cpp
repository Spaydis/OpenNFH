#include "opennfh/simulation/replay.hpp"

#include <charconv>
#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "opennfh/simulation/actions.hpp"
#include "opennfh/simulation/input.hpp"
#include "opennfh/simulation/neighbor_ai.hpp"

namespace opennfh::simulation {

namespace {

Error error(std::string message, std::size_t line = 0) {
    Error result;
    result.code = ErrorCode::Format;
    result.message = std::move(message);
    result.line = line;
    return result;
}

std::string_view action_token(InputAction action) {
    switch (action) {
    case InputAction::Unknown: return "unknown";
    case InputAction::PointerClick: return "pointer_click";
    case InputAction::ScrollLeft: return "scroll_left";
    case InputAction::ScrollRight: return "scroll_right";
    case InputAction::ScrollUp: return "scroll_up";
    case InputAction::ScrollDown: return "scroll_down";
    case InputAction::CenterWoody: return "center_woody";
    case InputAction::FocusNeighbor: return "focus_neighbor";
    case InputAction::Pause: return "pause";
    case InputAction::Screenshot: return "screenshot";
    case InputAction::Levelshot: return "levelshot";
    case InputAction::Quit: return "quit";
    case InputAction::StartCapture: return "start_capture";
    case InputAction::StopCapture: return "stop_capture";
    }
    return "unknown";
}

std::optional<InputAction> parse_action(std::string_view token) {
    constexpr InputAction actions[] = {
        InputAction::PointerClick, InputAction::ScrollLeft, InputAction::ScrollRight,
        InputAction::ScrollUp, InputAction::ScrollDown, InputAction::CenterWoody,
        InputAction::FocusNeighbor, InputAction::Pause, InputAction::Screenshot,
        InputAction::Levelshot, InputAction::Quit, InputAction::StartCapture,
        InputAction::StopCapture,
    };
    for (const auto action : actions) {
        if (action_token(action) == token) {
            return action;
        }
    }
    return std::nullopt;
}

template <typename Integer>
std::optional<Integer> parse_integer(std::string_view token) {
    Integer result = 0;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) {
        return std::nullopt;
    }
    return result;
}

void mix_byte(std::uint64_t& hash, std::uint8_t byte) {
    hash ^= byte;
    hash *= 1099511628211ull;
}

void mix_u64(std::uint64_t& hash, std::uint64_t value) {
    for (int index = 0; index < 8; ++index) {
        mix_byte(hash, static_cast<std::uint8_t>(value >> (index * 8)));
    }
}

void mix_string(std::uint64_t& hash, std::string_view value) {
    mix_u64(hash, value.size());
    for (const auto character : value) {
        mix_byte(hash, static_cast<std::uint8_t>(character));
    }
}

}  // namespace

Result<Replay> read_replay(std::istream& input) {
    std::string line;
    if (!std::getline(input, line)) {
        return Result<Replay>::failure(error("replay header is missing", 1));
    }
    std::istringstream header(line);
    std::string keyword;
    std::uint32_t version = 0;
    if (!(header >> keyword >> version) || keyword != "version" || version != 1) {
        return Result<Replay>::failure(error("unsupported replay header", 1));
    }

    Replay replay{version, {}};
    std::size_t line_number = 1;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty()) {
            continue;
        }
        std::istringstream row(line);
        std::string tick_token;
        std::string action_token_value;
        std::string x_token;
        std::string y_token;
        std::string target;
        std::string action_name;
        if (!(row >> tick_token >> action_token_value >> x_token >> y_token)) {
            return Result<Replay>::failure(error("malformed replay event", line_number));
        }
        const auto tick = parse_integer<Tick>(tick_token);
        const auto action = parse_action(action_token_value);
        const auto x = parse_integer<int>(x_token);
        const auto y = parse_integer<int>(y_token);
        if (!tick.has_value() || !action.has_value() || !x.has_value() || !y.has_value()) {
            return Result<Replay>::failure(error("invalid replay event value", line_number));
        }
        if (row >> target && target == "-") {
            target.clear();
        }
        if (row >> action_name && action_name == "-") {
            action_name.clear();
        }
        std::string extra;
        if (row >> extra) {
            return Result<Replay>::failure(error("replay event has too many fields", line_number));
        }
        replay.events.push_back(InputEvent{
            *tick, *action, {*x, *y}, std::move(target), std::move(action_name)});
    }
    return Result<Replay>::success(std::move(replay));
}

void write_replay(std::ostream& output, const Replay& replay) {
    output << "version " << replay.version << '\n';
    for (const auto& event : replay.events) {
        output << event.tick << ' ' << action_token(event.action) << ' '
               << event.cursor.x << ' ' << event.cursor.y << ' '
               << (event.target.empty() ? "-" : event.target) << ' '
               << (event.action_name.empty() ? "-" : event.action_name) << '\n';
    }
}

Result<ReplayRunResult> run_replay(
    WorldState& world,
    const Replay& replay,
    const ReplayRunOptions& options) {
    std::map<EntityId, ActionTransaction> active_actions;
    std::size_t noise_cursor = world.emitted_noise.size();
    Tick current_tick = 0;
    Tick previous_event_tick = 0;
    bool has_previous_event = false;
    bool paused = false;
    bool stopped_by_quit = false;
    std::size_t processed_events = 0;

    const auto advance_tick = [&](Tick tick) {
        if (paused) {
            return;
        }
        for (auto action = active_actions.begin(); action != active_actions.end();) {
            advance_action(world, action->second, tick);
            if (action->second.committed) {
                action = active_actions.erase(action);
            } else {
                ++action;
            }
        }
        while (noise_cursor < world.emitted_noise.size()) {
            dispatch_noise(world, world.emitted_noise[noise_cursor], tick);
            ++noise_cursor;
        }
        update_neighbor_ai(world, tick);
    };

    const auto advance_to = [&](Tick target) {
        while (current_tick < target) {
            ++current_tick;
            advance_tick(current_tick);
        }
    };

    for (const auto& event : replay.events) {
        if (has_previous_event && event.tick < previous_event_tick) {
            return Result<ReplayRunResult>::failure(
                error("replay events are not ordered by tick"));
        }
        previous_event_tick = event.tick;
        has_previous_event = true;
        advance_to(event.tick);

        if (event.action == InputAction::Pause) {
            paused = !paused;
        } else if (event.action == InputAction::Quit) {
            stopped_by_quit = true;
            ++processed_events;
            break;
        } else if (event.action == InputAction::PointerClick) {
            if (event.target.empty()) {
                if (options.strict_inputs) {
                    return Result<ReplayRunResult>::failure(
                        error("pointer replay event has no target"));
                }
            } else {
                if (options.controlled_actor == 0) {
                    return Result<ReplayRunResult>::failure(
                        error("replay has no controlled actor"));
                }
                const auto target = resolve_target(world, event.target);
                if (!target.has_value()) {
                    return Result<ReplayRunResult>::failure(target.error());
                }
                const auto request = action_request_for(
                    world, options.controlled_actor, target.value(), event.action_name);
                if (!request.has_value()) {
                    return Result<ReplayRunResult>::failure(request.error());
                }
                const auto started = begin_action(world, request.value(), event.tick);
                if (!started.has_value()) {
                    return Result<ReplayRunResult>::failure(started.error());
                }
                active_actions.emplace(started.value().actor, started.value());
            }
        }
        ++processed_events;
    }

    if (!stopped_by_quit) {
        for (std::uint64_t index = 0; index < options.tail_ticks; ++index) {
            ++current_tick;
            advance_tick(current_tick);
        }
    }

    ReplayRunResult result;
    result.final_tick = current_tick;
    result.processed_events = processed_events;
    result.stopped_by_quit = stopped_by_quit;
    result.paused = paused;
    result.snapshot = SimulationSnapshot{current_tick, world.entities, world.quotas};
    result.snapshot_hash = hash_snapshot(result.snapshot);
    return Result<ReplayRunResult>::success(std::move(result));
}

std::uint64_t hash_snapshot(const SimulationSnapshot& snapshot) {
    std::uint64_t hash = 1469598103934665603ull;
    mix_u64(hash, snapshot.tick);
    mix_u64(hash, snapshot.entities.size());
    for (const auto& entity : snapshot.entities) {
        mix_u64(hash, entity.id);
        mix_string(hash, entity.kind);
        mix_string(hash, entity.room);
        mix_u64(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(entity.position.x)));
        mix_u64(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(entity.position.y)));
        mix_u64(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(entity.layer)));
        mix_byte(hash, entity.active ? 1 : 0);
    }
    mix_u64(hash, snapshot.quotas.size());
    for (const auto& [name, quota] : snapshot.quotas) {
        mix_string(hash, name);
        mix_u64(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(quota)));
    }
    return hash;
}

}  // namespace opennfh::simulation
