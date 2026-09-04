#include "opennfh/simulation/replay.hpp"

#include <charconv>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

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
        replay.events.push_back(InputEvent{*tick, *action, {*x, *y}, std::move(target)});
    }
    return Result<Replay>::success(std::move(replay));
}

void write_replay(std::ostream& output, const Replay& replay) {
    output << "version " << replay.version << '\n';
    for (const auto& event : replay.events) {
        output << event.tick << ' ' << action_token(event.action) << ' '
               << event.cursor.x << ' ' << event.cursor.y << ' '
               << (event.target.empty() ? "-" : event.target) << '\n';
    }
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
