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
    Unknown,
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
    std::string action_name;
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

struct ReplayRunOptions {
    EntityId controlled_actor{0};
    std::uint64_t tail_ticks{0};
    bool strict_inputs{true};
};

struct ReplayRunResult {
    Tick final_tick{0};
    std::size_t processed_events{0};
    bool stopped_by_quit{false};
    bool paused{false};
    SimulationSnapshot snapshot;
    std::uint64_t snapshot_hash{0};
};

[[nodiscard]] Result<Replay> read_replay(std::istream& input);
void write_replay(std::ostream& output, const Replay& replay);
[[nodiscard]] Result<ReplayRunResult> run_replay(
    WorldState& world,
    const Replay& replay,
    const ReplayRunOptions& options = {});
[[nodiscard]] std::uint64_t hash_snapshot(const SimulationSnapshot& snapshot);

}  // namespace opennfh::simulation
