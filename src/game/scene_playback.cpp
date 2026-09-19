#include "game/scene_playback.hpp"
#include <stdexcept>

namespace weatherwar {
ScenePlaybackPlan plan_scene_playback(const FirstTurn& initial,
                                      const audio::StartupSequence& round_tone) {
    if (!initial.playing_scene()) throw std::invalid_argument("No automatic scene to plan");
    ScenePlaybackPlan plan;
    auto following = initial;
    while (following.playing_scene()) {
        if (plan.scenes.size() >= 9) throw std::logic_error("Unbounded automatic scene chain");
        const auto& events = following.scene_events();
        plan.scenes.push_back(schedule_events(events));
        const auto& timing = plan.scenes.back();
        for (std::size_t i = 0; i < events.size(); ++i)
            if (events[i].kind == AttackEventKind::sid_write)
                plan.sound.events.push_back({plan.animation_end + timing.event_cycles[i],
                    static_cast<std::uint8_t>(events[i].address - 0xd400), events[i].value});
        plan.animation_end += timing.end_cycle;
        following.finish_scene();
    }
    plan.sound.end_cycle = plan.animation_end;
    if (following.phase() == FirstTurnPhase::round_tone) {
        for (const auto& event : round_tone.events)
            plan.sound.events.push_back({plan.animation_end + event.cycle, event.reg, event.value});
        plan.sound.end_cycle += round_tone.end_cycle;
    }
    plan.round_pause_cycles = calibrated_loop_cycles(following.round_pause_loops());
    return plan;
}
}
