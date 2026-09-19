#pragma once
#include "weather/weather_attack.hpp"

namespace weatherwar {
// BASIC 56-58 and SID 239-240. Choice/charge (59,77-78) belongs to the caller
// and must consume RNG only after this sequence finishes.
[[nodiscard]] WeatherAttackResult simulate_nature_intro(const TextScreen& initial);
}
