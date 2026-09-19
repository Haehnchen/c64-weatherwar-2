#pragma once
#include "audio/startup_sequence.hpp"
#include "weather/attack_timing.hpp"
#include "game/first_turn.hpp"
#include <deque>

namespace weatherwar {
struct ScenePlaybackPlan {
    std::deque<AttackSchedule> scenes;
    audio::StartupSequence sound{};
    std::uint64_t animation_end = 0;
    std::uint64_t round_pause_cycles = 0;
};

// Plan only automatic transitions on a value copy. The live controller is
// advanced by the platform at each scene's presentation boundary.
ScenePlaybackPlan plan_scene_playback(const FirstTurn& initial,
                                      const audio::StartupSequence& round_tone);
}
