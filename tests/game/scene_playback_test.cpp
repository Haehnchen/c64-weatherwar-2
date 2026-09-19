#include "game/scene_playback.hpp"
#include "scenes/opening.hpp"
#include "support/check.hpp"
#include <iostream>

using namespace weatherwar;
bool same_frame(const CharacterFrame& a, const CharacterFrame& b) {
    if (a.screen != b.screen || a.colors != b.colors || a.charset != b.charset ||
        a.background != b.background || a.border != b.border) return false;
    for (unsigned i = 0; i < a.sprites.size(); ++i) {
        const auto& x = a.sprites[i]; const auto& y = b.sprites[i];
        if (x.data != y.data || x.x != y.x || x.y != y.y || x.enabled != y.enabled ||
            x.expand_x != y.expand_x || x.expand_y != y.expand_y ||
            x.behind_character != y.behind_character || x.color != y.color) return false;
    }
    return true;
}
FirstTurn initial_turn() {
    Opening opening({}, {}); opening.show(OpeningPage::question); opening.type('N');
    Setup setup(opening.screen(), {}, false); setup.finish_tone();
    for (char c : std::string("ALICE")) setup.type(c);
    setup.confirm(); setup.finish_tone();
    for (char c : std::string("BOB")) setup.type(c);
    setup.confirm(); setup.finish_tone(); setup.finish_tone();
    return FirstTurn(setup);
}
void verify_plan(const FirstTurn& initial, const audio::StartupSequence& tone,
                 unsigned scenes, unsigned stores = 0) {
    const auto before = initial;
    const auto plan = plan_scene_playback(initial, tone);
    test_support::check(plan.scenes.size() == scenes);
    test_support::check(initial.phase() == before.phase() && initial.random_seed() == before.random_seed() &&
          initial.first_name() == before.first_name() && initial.second_name() == before.second_name() &&
          initial.left_score() == before.left_score() && initial.right_score() == before.right_score() &&
          same_frame(initial.screen().frame, before.screen().frame));
    auto live = initial;
    auto direct = initial;
    std::uint64_t offset = 0;
    std::size_t store = 0;
    for (const auto& schedule : plan.scenes) {
        const auto events = live.scene_events();
        test_support::check(events.size() == schedule.event_cycles.size());
        for (std::size_t i = 0; i < events.size(); ++i) {
            live.apply_scene_event(i);
            if (events[i].kind == AttackEventKind::sid_write) {
                const auto& actual = plan.sound.events.at(store++);
                test_support::check(actual.cycle == offset + schedule.event_cycles[i] &&
                      actual.reg == events[i].address - 0xd400 && actual.value == events[i].value);
            }
        }
        offset += schedule.end_cycle;
        live.finish_scene(); direct.finish_scene();
        test_support::check(live.phase() == direct.phase() && live.random_seed() == direct.random_seed() &&
              live.first_name() == direct.first_name() && live.second_name() == direct.second_name() &&
              live.left_score() == direct.left_score() && live.right_score() == direct.right_score() &&
              same_frame(live.screen().frame, direct.screen().frame));
    }
    test_support::check(offset == plan.animation_end);
    if (live.phase() == FirstTurnPhase::round_tone) {
        for (const auto& event : tone.events) {
            const auto& actual = plan.sound.events.at(store++);
            test_support::check(actual.cycle == offset + event.cycle && actual.reg == event.reg && actual.value == event.value);
        }
        test_support::check(plan.sound.end_cycle == offset + tone.end_cycle);
    } else test_support::check(plan.sound.end_cycle == offset);
    test_support::check(store == plan.sound.events.size() && (!stores || store == stores));
    // SidTimeline preserves vector order for simultaneous stores. A fallback
    // END and the next captured tone's first store can share a cycle until
    // the intervening BASIC dispatch time is measured; do not invent a gap.
    for (std::size_t i = 1; i < store; ++i)
        test_support::check(plan.sound.events[i - 1].cycle <= plan.sound.events[i].cycle);
}
void test_scene_plans_preserve_state_and_store_order() {
    const auto tone = audio::builtin_sequence(audio::SequenceId::round);
    auto turn = initial_turn();
    bool nature_checked = false;
    for (unsigned transition = 0; transition < 256 && turn.phase() != FirstTurnPhase::result_effect; ++transition) {
        if (turn.phase() == FirstTurnPhase::round_tone) turn.finish_tone();
        else if (turn.waiting_for_weapon()) {
            turn.type('H'); turn.confirm(); turn.type('0'); turn.confirm();
        } else if (turn.playing_scene()) {
            if (turn.phase() == FirstTurnPhase::nature_intro && !nature_checked) {
                verify_plan(turn, tone, 2); nature_checked = true;
            }
            turn.finish_scene();
        } else test_support::check(false);
    }
    test_support::check(nature_checked && turn.phase() == FirstTurnPhase::result_effect);
    verify_plan(turn, tone, 1, 207);

    // A complete replay answer already waiting at plan time is part of the
    // copied controller state.  It must extend the automatic chain through
    // result, replay music and board, including the following round tone.
    auto prebuffered_replay = turn;
    prebuffered_replay.type('Y'); prebuffered_replay.confirm();
    verify_plan(prebuffered_replay, tone, 3, 592); // 207 result + 365 music + 10 board + 10 round.

    // Input arriving after the snapshot cannot change its planned prefix.
    // The result-only plan completes first; the live controller then consumes
    // Y/RETURN and produces a fresh replay/board/round plan without losing the
    // round tone.
    auto late_replay = turn;
    const auto result_only = plan_scene_playback(late_replay, tone);
    test_support::check(result_only.scenes.size() == 1 && result_only.sound.events.size() == 207 &&
          result_only.sound.end_cycle == result_only.animation_end);
    late_replay.type('Y'); late_replay.confirm();
    late_replay.finish_scene();
    test_support::check(late_replay.phase() == FirstTurnPhase::replay_music);
    verify_plan(late_replay, tone, 2, 385); // 365 music + 10 board + 10 round.

    const auto result = prepare_match_result(turn.screen(), turn.left_structure(), turn.right_structure(),
                                             turn.first_name(), turn.second_name());
    for (std::size_t i = 0; i < turn.scene_events().size(); ++i) turn.apply_scene_event(i);
    test_support::check(same_frame(turn.screen().frame, result.screen.frame));
    turn.finish_scene(); turn.type('Y'); turn.confirm();
    verify_plan(turn, tone, 2, 385); // 365 music + 10 board + 10 round stores.
    turn.finish_scene();
    test_support::check(turn.phase() == FirstTurnPhase::replay_board);
    const auto sprites = turn.screen().frame.sprites;
    for (std::size_t i = 0; i < turn.scene_events().size(); ++i) {
        turn.apply_scene_event(i);
        for (unsigned s = 0; s < sprites.size(); ++s) {
            auto actual = turn.screen().frame;
            auto expected = actual;
            expected.sprites[s] = sprites[s]; expected.sprites[s].enabled = i >= 2;
            test_support::check(same_frame(actual, expected));
        }
    }
}

int main() {
    test_scene_plans_preserve_state_and_store_order();
    std::cout << "Automatic scene planning preserves live state, sprite state, exact store order and cumulative timing.\n";
}
