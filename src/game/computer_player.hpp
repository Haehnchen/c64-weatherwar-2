#pragma once

#include "basic/basic_random.hpp"
#include "weather/weather_attack.hpp"
#include "video/text_screen.hpp"

namespace weatherwar {

// Visual/attacker side. Right is BASIC CT=1 (Y$); left is CT=2 (Z$).
enum class ComputerSide {
    left,
    right,
};

// BASIC numeric scalars which are not reset by replay through line 26.
// Before each decision the caller supplies P=G as left by lines 113-117.
struct AiMemory {
    double li = 0.0;
    double lx = 0.0;
    double p = 0.0;
    double pq = 0.0;
    double qq = 0.0;
    double qp = 0.0;
};

struct ComputerDecision {
    WeatherWeapon weapon = WeatherWeapon::hail;
    double charge = 0.0; // A1 before the clamps at BASIC 80-81.
    double wind = 0.0;   // EE after the optional adjustments at 196/213.
    int attempts = 0;    // Final PB; direct choices use zero.
    int target_x = -1;   // Scan coordinate, or -1 when no scan hit occurred.
    int target_y = -1;
};

// Native BASIC 180-214 selection and aim. The TextScreen is read only and
// RNG/AiMemory are advanced in source order.
[[nodiscard]] ComputerDecision choose_computer_attack(
    ComputerSide side, int round, double aa, double wind,
    const TextScreen& screen, BasicRandom& random, AiMemory& memory);

} // namespace weatherwar
