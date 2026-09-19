#pragma once

#include "video/text_screen.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace weatherwar {

enum class WeatherWeapon {
    hail,
    rain,
    lightning,
    tornado,
};

enum class AttackEventKind {
    screen_write,
    sid_write,
    delay,
    text_screen,
    vic_write,
};

// One observable operation, in BASIC execution order. `basic_line` names the
// line that performs the operation (for example 135, not its GOSUB call site).
// Delay events describe source loop counts; converting them to wall-clock time
// belongs to the platform timeline and is deliberately not guessed here.
// BASIC PRINT is represented by one text_screen snapshot and is not duplicated
// as synthetic screen_write events.
struct AttackEvent {
    AttackEventKind kind{};
    int basic_line = 0;
    std::uint16_t address = 0;
    std::uint8_t value = 0;
    int delay_loop_count = 0;
    std::optional<TextScreen> text_screen;
};

struct WeatherAttackInput {
    TextScreen screen;
    WeatherWeapon weapon = WeatherWeapon::hail;
    double aa = 0.0;
    double wind_ee = 0.0;
    double charge_a1 = 0.0;
    int mm = 1;
    int ww = 0;
    int nn = 0;
};

struct WeatherAttackResult {
    TextScreen screen;
    std::vector<AttackEvent> events;
    int ww = 0;
    int nn = 0;
    double aa = 0.0;
    double charge_a1 = 0.0;
};

// Native implementation of the four weather paths at BASIC 83-139. It consumes
// the already-rendered screen state immediately before line 83. Charge input
// and the line-37 transition to the following turn remain outside this unit.
[[nodiscard]] WeatherAttackResult simulate_weather_attack(const WeatherAttackInput& input);

} // namespace weatherwar
