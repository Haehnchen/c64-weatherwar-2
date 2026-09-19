#pragma once
#include "weather/weather_attack.hpp"

namespace weatherwar {
// BASIC 227–233 with the effective DATA stream from 234–238. Register stores
// only: timestamps belong to the measured playback schedule, not this rule.
// Deliberately no D418 write, SID reset or RNG initialization on replay.
std::vector<AttackEvent> replay_music_events();
}
