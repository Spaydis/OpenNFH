#include <cassert>

#include "opennfh/simulation/clock.hpp"

int main() {
    opennfh::simulation::LogicClock clock;
    assert(clock.fps == 12);
    assert(clock.period_ms == 83);
    assert(opennfh::simulation::consume_logic_ticks(clock, 82) == 0);
    assert(opennfh::simulation::consume_logic_ticks(clock, 1) == 1);
    assert(opennfh::simulation::consume_logic_ticks(clock, 166) == 2);
    opennfh::simulation::set_logic_fps(clock, 0);
    assert(clock.fps == 1);
    assert(clock.period_ms == 1000);
    opennfh::simulation::set_logic_fps(clock, 100);
    assert(clock.fps == 60);
    assert(clock.period_ms == 16);
    return 0;
}
