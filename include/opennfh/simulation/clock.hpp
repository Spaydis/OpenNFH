#pragma once

#include <cstdint>

namespace opennfh::simulation {

struct LogicClock {
    int fps{12};
    int period_ms{83};
    std::uint64_t accumulator_ms{0};
};

void set_logic_fps(LogicClock& clock, int requested);
int consume_logic_ticks(LogicClock& clock, std::uint64_t elapsed_ms);

}  // namespace opennfh::simulation
