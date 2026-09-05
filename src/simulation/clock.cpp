#include "opennfh/simulation/clock.hpp"

#include <algorithm>
#include <limits>

namespace opennfh::simulation {

void set_logic_fps(LogicClock& clock, int requested) {
    clock.fps = std::clamp(requested, 1, 60);
    clock.period_ms = 1000 / clock.fps;
    if (clock.accumulator_ms >= static_cast<std::uint64_t>(clock.period_ms)) {
        clock.accumulator_ms %= static_cast<std::uint64_t>(clock.period_ms);
    }
}

int consume_logic_ticks(LogicClock& clock, std::uint64_t elapsed_ms) {
    if (clock.fps < 1 || clock.fps > 60 || clock.period_ms <= 0) {
        set_logic_fps(clock, clock.fps);
    }
    const auto period = static_cast<std::uint64_t>(clock.period_ms);
    if (elapsed_ms > std::numeric_limits<std::uint64_t>::max() - clock.accumulator_ms) {
        clock.accumulator_ms = std::numeric_limits<std::uint64_t>::max();
    } else {
        clock.accumulator_ms += elapsed_ms;
    }
    const auto ticks = clock.accumulator_ms / period;
    clock.accumulator_ms %= period;
    return ticks > static_cast<std::uint64_t>(std::numeric_limits<int>::max())
        ? std::numeric_limits<int>::max()
        : static_cast<int>(ticks);
}

}  // namespace opennfh::simulation
