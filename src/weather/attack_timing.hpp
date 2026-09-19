#pragma once
#include "weather/weather_attack.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace weatherwar {
struct AttackSchedule {
    std::vector<std::uint64_t> event_cycles;
    std::uint64_t end_cycle = 0;
    unsigned fallback_intervals = 0;
    // Snapshot/delay boundaries interpolated inside measured SID intervals.
    unsigned interpolated_boundaries = 0;
    bool nature_sid_calibrated = false;
};
// Calibrated operation timing, not a cycle-exact BASIC interpreter.
AttackSchedule schedule_attack(const WeatherAttackResult& attack);
AttackSchedule schedule_events(std::span<const AttackEvent> events);
// PAL timing estimate for BASIC FOR/NEXT loops.
std::uint64_t calibrated_loop_cycles(unsigned count);
}
