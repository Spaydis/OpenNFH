#pragma once

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include "opennfh/core/result.hpp"
#include "opennfh/simulation/world.hpp"

namespace opennfh::simulation {

enum class InputAction {
    PointerClick,
    ScrollLeft,
    ScrollRight,
    ScrollUp,
    ScrollDown,
    CenterWoody,
    FocusNeighbor,
    Pause,
    Screenshot,
    Levelshot,
    Quit,
    StartCapture,
    StopCapture,
};

struct InputEvent {
    Tick tick{0};
    InputAction action{InputAction::PointerClick};
    Vec2i cursor;
    std::string target;
};

struct Replay {
    std::uint32_t version{1};
    std::vector<InputEvent> events;
};

struct SimulationSnapshot {
    Tick tick{0};
    std::vector<EntityState> entities;
    std::map<std::string, int> quotas;
};

[[nodiscard]] Result<Replay> read_replay(std::istream& input);
void write_replay(std::ostream& output, const Replay& replay);
[[nodiscard]] std::uint64_t hash_snapshot(const SimulationSnapshot& snapshot);

}  // namespace opennfh::simulation
