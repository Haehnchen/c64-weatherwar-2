#include "game/first_turn.hpp"
#include "scenes/opening.hpp"
#include "weather/attack_timing.hpp"
#include "support/check.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace weatherwar;

weatherwar::Setup board(const std::string& first = "ALICE", const std::string& second = "BOB") {
    using namespace weatherwar;
    Opening opening({}, {}); opening.show(OpeningPage::question); opening.type('N');
    Setup setup(opening.screen(), {}, false); setup.finish_tone();
    for (char c : first) setup.type(c);
    setup.confirm(); setup.finish_tone();
    for (char c : second) setup.type(c);
    setup.confirm(); setup.finish_tone(); setup.finish_tone();
    return setup;
}
void row_text(const weatherwar::TextScreen& screen, unsigned row,
              unsigned column, std::string_view expected) {
    for (unsigned i = 0; i < expected.size(); ++i) {
        auto code = static_cast<unsigned char>(expected[i]);
        if (code >= 'A' && code <= 'Z') code -= 64;
        test_support::check(screen.frame.screen.at(row * 40 + column + i) == code);
    }
}
bool contains_text(const weatherwar::TextScreen& screen, std::string_view expected) {
    for (unsigned row = 0; row < 25; ++row) {
        for (unsigned column = 0; column + expected.size() <= 40; ++column) {
            bool match = true;
            for (unsigned i = 0; i < expected.size(); ++i) {
                auto code = static_cast<unsigned char>(expected[i]);
                if (code >= 'A' && code <= 'Z') code -= 64;
                if (screen.frame.screen[row * 40 + column + i] != code) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
    }
    return false;
}

void test_human_turn_and_input() {
    FirstTurn turn(board());
    test_support::check(turn.phase() == FirstTurnPhase::round_tone);
    turn.type('H'); test_support::check(turn.input().empty());
    test_support::check(turn.cloud() >= 0 && turn.cloud() < 26);
    test_support::check(turn.wind() >= -50 && turn.wind() <= 50);
    test_support::check(turn.left_structure() > 0 && turn.right_structure() > 0);
    // Original state at BASIC 63.
    test_support::check(turn.left_structure() == 43 && turn.right_structure() == 43);
    test_support::check(turn.cloud() == 1.2193645038641989 && turn.wind() == -41);
    test_support::check(turn.random_seed() == BasicRandom::Seed{0x80,0x53,0xe7,0x04,0x89});
    turn.finish_tone(); test_support::check(turn.waiting_for_weapon() && turn.input() == "H");
    turn.backspace(); test_support::check(turn.input().empty());
    const auto seed = turn.random_seed();
    turn.type('x'); turn.confirm(); test_support::check(turn.waiting_for_weapon());
    test_support::check(turn.input().empty() && turn.random_seed() == seed);
    turn.type('h'); turn.type('l'); turn.backspace(); turn.confirm();
    test_support::check(turn.weapon() == "HAIL" && turn.phase() == FirstTurnPhase::charge_preview);
    test_support::check(turn.random_seed() == seed); test_support::check(turn.input().empty());
    turn.type('0'); turn.confirm(); test_support::check(turn.phase() == FirstTurnPhase::attack);
    // A key received while the attack is running is held for the next human
    // prompt; cancel it here so this test's later H input remains isolated.
    turn.type('Z'); turn.backspace(); test_support::check(turn.input() == "0");
    test_support::check(!turn.attack_result().events.empty());
    const auto schedule = schedule_attack(turn.attack_result());
    test_support::check(schedule.event_cycles.size() == turn.attack_result().events.size());
    test_support::check(std::is_sorted(schedule.event_cycles.begin(), schedule.event_cycles.end()));
    test_support::check(schedule.end_cycle >= schedule.event_cycles.back());
    test_support::check(schedule.fallback_intervals == 0);
    test_support::check(schedule_attack(turn.attack_result()).event_cycles == schedule.event_cycles);
    for (std::size_t i = 0; i < schedule.event_cycles.size(); ++i) turn.apply_attack_event(i);
    test_support::check(turn.screen().frame.screen == turn.attack_result().screen.frame.screen);
    turn.finish_attack(); test_support::check(turn.phase() == FirstTurnPhase::round_tone);
    test_support::check(turn.turn_mode() == 2 && turn.attacker() == 0 && turn.round() == 1);
    test_support::check(turn.round_pause_loops() == 250);
    turn.finish_tone(); test_support::check(turn.waiting_for_weapon());
    test_support::check(turn.left_structure() == 43 && turn.right_structure() == 43 && turn.wind() == -29);
    test_support::check(turn.random_seed() == BasicRandom::Seed{0x80,0x12,0xaa,0xa3,0xa3});
    for (const auto& [charge, expected] : {std::pair{"999", 150.0}, {"-999", -150.0},
                                           {"NO", 0.0}, {"1E-99", 0.0},
                                           {"12.5", 12.5}}) {
        FirstTurn charged(board()); charged.finish_tone(); charged.type('R'); charged.confirm();
        for (const char* p = charge; *p; ++p) charged.type(*p);
        charged.confirm(); test_support::check(charged.phase() == FirstTurnPhase::attack && charged.charge() == expected);
    }
    for (const auto& [charge, expected_bb, expected_extra, expected_charge] : {
             std::tuple<std::string, std::string, bool, double>{"\"50\"", "50", false, 50.0},
             std::tuple<std::string, std::string, bool, double>{"50,60", "50", true, 50.0},
             std::tuple<std::string, std::string, bool, double>{"  50", "50", false, 50.0},
             std::tuple<std::string, std::string, bool, double>{"50  ", "50", false, 50.0}}) {
        FirstTurn parsed(board()); parsed.finish_tone(); parsed.type('H'); parsed.confirm();
        for (char c : charge) parsed.type(c);
        parsed.confirm();
        test_support::check(parsed.phase() == FirstTurnPhase::attack && parsed.charge() == expected_charge);
        test_support::check(parsed.charge_bb() == expected_bb && parsed.charge_extra_ignored() == expected_extra);
    }
    FirstTurn empty_return(board()); empty_return.finish_tone();
    empty_return.type('H'); empty_return.confirm(); empty_return.confirm();
    test_support::check(empty_return.phase() == FirstTurnPhase::attack &&
          empty_return.charge() == 0.0 && empty_return.charge_bb() == "\xBF" &&
          !empty_return.charge_extra_ignored());
    FirstTurn overflow(board()); overflow.finish_tone(); overflow.type('H'); overflow.confirm();
    const auto overflow_seed = overflow.random_seed();
    const auto board_screen = overflow.screen().frame.screen;
    const auto board_colors = overflow.screen().frame.colors;
    const auto board_rows_unchanged = [&]() {
        for (unsigned row = 0; row <= 20; ++row) {
            const auto offset = row * 40;
            if (!std::equal(board_screen.begin() + offset, board_screen.begin() + offset + 40,
                            overflow.screen().frame.screen.begin() + offset) ||
                !std::equal(board_colors.begin() + offset, board_colors.begin() + offset + 40,
                            overflow.screen().frame.colors.begin() + offset)) return false;
        }
        return true;
    };
    const auto check_overflow_retry = [&](const std::string& invalid) {
        for (const char c : invalid) overflow.type(c);
        overflow.confirm();
        test_support::check(overflow.phase() == FirstTurnPhase::charge_preview &&
              overflow.weapon() == "HAIL" && overflow.input().empty() &&
              overflow.random_seed() == overflow_seed && overflow.charge() == 0.0 &&
              !overflow.playing_scene() && overflow.waiting_for_input() &&
              board_rows_unchanged() && contains_text(overflow.screen(), "?REDO FROM START"));
    };
    for (unsigned retry = 0; retry < 10; ++retry) {
        check_overflow_retry("1E39");
    }
    check_overflow_retry("1E" + std::string(90, '9'));
    overflow.type('0'); overflow.confirm();
    test_support::check(overflow.phase() == FirstTurnPhase::attack &&
          overflow.weapon() == "HAIL" && overflow.charge() == 0.0 &&
          std::any_of(overflow.attack_result().events.begin(), overflow.attack_result().events.end(),
                      [](const auto& event) { return event.kind == AttackEventKind::sid_write; }));
    FirstTurn long_charge(board()); long_charge.finish_tone(); long_charge.type('H'); long_charge.confirm();
    const auto long_charge_screen = long_charge.screen().frame.screen;
    const auto long_charge_colors = long_charge.screen().frame.colors;
    for (unsigned i = 0; i < 78; ++i) long_charge.type('0');
    long_charge.type('1');
    test_support::check(long_charge.input().size() == 79);
    for (unsigned row = 0; row <= 20; ++row) {
        const auto offset = row * 40;
        test_support::check(std::equal(long_charge_screen.begin() + offset,
                                       long_charge_screen.begin() + offset + 40,
                                       long_charge.screen().frame.screen.begin() + offset));
        test_support::check(std::equal(long_charge_colors.begin() + offset,
                                       long_charge_colors.begin() + offset + 40,
                                       long_charge.screen().frame.colors.begin() + offset));
    }
    long_charge.confirm();
    test_support::check(long_charge.phase() == FirstTurnPhase::attack &&
          long_charge.weapon() == "HAIL" && long_charge.charge() == 1.0 &&
          std::any_of(long_charge.attack_result().events.begin(), long_charge.attack_result().events.end(),
                      [](const auto& event) { return event.kind == AttackEventKind::sid_write; }));
    for (char choice : std::string("HLRT")) {
        FirstTurn selected(board()); selected.finish_tone(); selected.type(choice); selected.confirm();
        test_support::check(selected.phase() == FirstTurnPhase::charge_preview);
        selected.type('0'); selected.confirm();
        test_support::check(selected.phase() == FirstTurnPhase::attack);
        if (choice == 'L') {
            std::array<std::uint8_t, 2048> alternate{}; alternate.fill(0xa5);
            selected.set_alternate_charset(alternate);
            bool switched = false;
            for (std::size_t i = 0; i < selected.attack_result().events.size(); ++i) {
                const auto& event = selected.attack_result().events[i];
                selected.apply_attack_event(i);
                if (event.kind == AttackEventKind::vic_write && event.address == 53272 && event.value == 22) {
                    test_support::check(selected.screen().frame.charset == alternate); switched = true;
                }
            }
            test_support::check(switched && selected.screen().frame.background == 0);
            test_support::check(selected.screen().frame.charset != alternate);
        }
    }
    FirstTurn quoted_weapon(board());
    quoted_weapon.finish_tone();
    for (const char c : std::string("  \"h\"  ")) quoted_weapon.type(c);
    quoted_weapon.confirm();
    test_support::check(quoted_weapon.phase() == FirstTurnPhase::charge_preview &&
          quoted_weapon.weapon() == "HAIL");
    for (char choice : std::string("SQ")) {
        FirstTurn special(board()); special.finish_tone(); special.type(choice); special.confirm();
        if (choice == 'S') {
            test_support::check(special.phase() == FirstTurnPhase::statistics && !special.waiting_for_input());
            test_support::check(special.statistics_delay_loops() == 5000);
            const auto seed = special.random_seed();
            special.type('H'); special.confirm();
            test_support::check(special.phase() == FirstTurnPhase::statistics);
            special.finish_statistics();
            test_support::check(special.phase() == FirstTurnPhase::charge_preview && special.weapon() == "HAIL");
            test_support::check(special.random_seed() == seed && special.input().empty());
        } else {
            test_support::check(special.phase() == FirstTurnPhase::ended && !special.waiting_for_input());
            test_support::check(special.screen().column == 0 && special.screen().row == 11 && special.screen().color == 0);
            for (const auto& sprite : special.screen().frame.sprites) test_support::check(!sprite.enabled);
        }
    }
}

void test_nature_turn_and_charge_selection() {
    // Natural ALICE/BOB HAIL-0 play reaches the first M=3 branch after the
    // sixteenth human action at MM=8. Lines 40-52, including their RNG and
    // round tone, have already completed before the line-56 intro begins.
    FirstTurn nature(board());
    unsigned human_actions = 0;
    while (true) {
        test_support::check(nature.phase() == FirstTurnPhase::round_tone);
        nature.finish_tone();
        if (nature.phase() == FirstTurnPhase::nature_intro) break;
        test_support::check(nature.waiting_for_weapon());
        nature.type('H'); nature.confirm(); nature.type('0'); nature.confirm();
        test_support::check(nature.phase() == FirstTurnPhase::attack && nature.charge() == 0.0);
        nature.finish_attack();
        test_support::check(++human_actions <= 16);
    }
    test_support::check(human_actions == 16 && nature.round() == 8 && nature.turn_mode() == 3);
    test_support::check(nature.round_pause_loops() == 0 && !nature.waiting_for_input());
    // BASIC46: both buildings are targets; no ATTACKER label during M=3.
    constexpr std::array<std::uint8_t, 8> nature_label{160, 20, 1, 18, 7, 5, 20, 160};
    test_support::check(std::equal(nature_label.begin(), nature_label.end(), nature.screen().frame.screen.begin() + 844));
    const auto intro_seed = nature.random_seed();
    const auto& intro = nature.attack_result();
    test_support::check(intro.events.size() == 290);
    test_support::check(std::count_if(intro.events.begin(), intro.events.end(), [](const auto& event) {
        return event.kind == AttackEventKind::sid_write;
    }) == 280);
    test_support::check(std::count_if(intro.events.begin(), intro.events.end(), [](const auto& event) {
        return event.kind == AttackEventKind::text_screen;
    }) == 5);
    test_support::check(std::count_if(intro.events.begin(), intro.events.end(), [](const auto& event) {
        return event.kind == AttackEventKind::delay;
    }) == 5);
    for (std::size_t i = 0; i < intro.events.size(); ++i) nature.apply_attack_event(i);
    test_support::check(nature.screen().frame.screen == intro.screen.frame.screen);
    test_support::check(nature.random_seed() == intro_seed); // Line 59 has not run yet.

    BasicRandom expected_choice(intro_seed);
    const auto choice_index = static_cast<unsigned>(std::floor(expected_choice.next_scaled(4)));
    constexpr std::array choice_names{"HAIL", "LIGHTNING", "RAIN", "TORNADO"};
    nature.finish_attack();
    test_support::check(nature.phase() == FirstTurnPhase::attack &&
          nature.random_seed() == expected_choice.seed() &&
          nature.weapon() == choice_names.at(choice_index));
    const double expected_nature_charge = nature.weapon() != "LIGHTNING" ? 0.0
        : nature.wind() < 1.0 ? -1.0 : nature.wind() > 1.0 ? 1.0 : 0.0;
    test_support::check(nature.charge() == expected_nature_charge && !nature.waiting_for_input());
    test_support::check(std::any_of(nature.attack_result().events.begin(), nature.attack_result().events.end(),
                      [](const auto& event) { return event.kind == AttackEventKind::sid_write; }));
    nature.finish_attack();
    test_support::check(nature.phase() == FirstTurnPhase::round_tone &&
          nature.turn_mode() == 1 && nature.round() == 9);

    // BASIC 77/78 use strict comparisons around EE=1. These fixed native
    // BASIC seeds naturally reach M=3 and select LIGHTNING at line 59.
    struct NatureLightningCase {
        BasicRandom::Seed seed;
        int wind;
        double charge;
        unsigned preceding_actions;
    };
    constexpr std::array lightning_cases{
        NatureLightningCase{{0x80, 0x4f, 0xc7, 0x00, 0x02}, -49, -1.0, 10},
        NatureLightningCase{{0x80, 0x4f, 0xc7, 0x03, 0xed},   1,  0.0, 22},
        NatureLightningCase{{0x80, 0x4f, 0xc7, 0x00, 0x00},  34,  1.0, 10},
    };
    for (const auto& test : lightning_cases) {
        FirstTurn boundary(board(), test.seed);
        unsigned actions = 0;
        while (true) {
            boundary.finish_tone();
            if (boundary.phase() == FirstTurnPhase::nature_intro) break;
            test_support::check(boundary.waiting_for_weapon());
            boundary.type('H'); boundary.confirm(); boundary.type('0'); boundary.confirm();
            test_support::check(boundary.phase() == FirstTurnPhase::attack);
            boundary.finish_attack(); ++actions;
        }
        test_support::check(actions == test.preceding_actions && boundary.wind() == test.wind);
        boundary.finish_attack();
        test_support::check(boundary.phase() == FirstTurnPhase::attack &&
              boundary.weapon() == "LIGHTNING" && boundary.charge() == test.charge);
    }

}

void test_match_result_and_replay() {
    // Native long-run regression only, not an original full-game acceptance.
    // No state injection: ordinary H0 inputs and automatic nature transitions
    // reach the source-mapped result effect within a finite budget.
    FirstTurn full_game(board());
    unsigned weather_actions = 0, nature_intros = 0;
    for (unsigned transition = 0; transition < 256 &&
         full_game.phase() != FirstTurnPhase::result_effect; ++transition) {
        switch (full_game.phase()) {
        case FirstTurnPhase::round_tone: full_game.finish_tone(); break;
        case FirstTurnPhase::weapon:
            full_game.type('H'); full_game.confirm();
            full_game.type('0'); full_game.confirm();
            break;
        case FirstTurnPhase::nature_intro:
            ++nature_intros; full_game.finish_attack(); break;
        case FirstTurnPhase::attack:
            ++weather_actions; full_game.finish_attack(); break;
        default: test_support::check(false);
        }
    }
    test_support::check(full_game.phase() == FirstTurnPhase::result_effect && full_game.playing_scene());
    test_support::check(weather_actions == 47 && nature_intros > 0);
    test_support::check(full_game.left_structure() == 0 && full_game.right_structure() == 11 &&
          full_game.round() == 23);
    test_support::check((full_game.random_seed() == BasicRandom::Seed{0x80, 0x5c, 0x16, 0x3a, 0x83}));
    const auto result_seed = full_game.random_seed();
    test_support::check(std::count_if(full_game.scene_events().begin(), full_game.scene_events().end(),
                        [](const auto& event) {
                            return event.kind == AttackEventKind::sid_write;
                        }) == 207);
    for (std::size_t i = 0; i < full_game.scene_events().size(); ++i)
        full_game.apply_scene_event(i);
    full_game.finish_scene();
    test_support::check(full_game.phase() == FirstTurnPhase::result_prompt &&
          full_game.right_score() == 1 && full_game.left_score() == 0 &&
          full_game.random_seed() == result_seed);
    // Re-finishing a completed scene must not award the winner twice.
    full_game.finish_scene();
    test_support::check(full_game.right_score() == 1);

    // Result-S shows the same statistics helper but returns to line 154,
    // without changing score or RNG.
    for (const char c : std::string("  \"s\"  ")) full_game.type(c);
    full_game.confirm();
    test_support::check(full_game.phase() == FirstTurnPhase::statistics);
    full_game.finish_statistics();
    test_support::check(full_game.phase() == FirstTurnPhase::result_prompt &&
          full_game.right_score() == 1 && full_game.random_seed() == result_seed);

    FirstTurn quit_result = full_game;
    quit_result.type('N'); quit_result.confirm();
    test_support::check(quit_result.phase() == FirstTurnPhase::ended &&
          quit_result.right_score() == 1 && quit_result.left_score() == 0 &&
          quit_result.random_seed() == result_seed);

    // Result-Y swaps names and their scores before the source music. Music
    // and board rebuild consume no RNG; only the subsequent begin() does.
    full_game.type('Y'); full_game.confirm();
    test_support::check(full_game.phase() == FirstTurnPhase::replay_music &&
          full_game.playing_scene() && full_game.scene_events().size() == 365);
    test_support::check(full_game.first_name() == "BOB" && full_game.second_name() == "ALICE" &&
          full_game.left_score() == 1 && full_game.right_score() == 0 &&
          full_game.random_seed() == result_seed);
    full_game.finish_scene();
    test_support::check(full_game.phase() == FirstTurnPhase::replay_board &&
          full_game.random_seed() == result_seed);
    test_support::check(full_game.scene_events().size() == 14 &&
          full_game.scene_events()[0].kind == AttackEventKind::vic_write &&
          full_game.scene_events()[0].address == 53269 &&
          full_game.scene_events()[0].value == 0 &&
          full_game.scene_events()[1].kind == AttackEventKind::text_screen &&
          full_game.scene_events()[2].kind == AttackEventKind::vic_write &&
          full_game.scene_events()[2].address == 53269 &&
          full_game.scene_events()[2].value == 255);
    test_support::check(std::count_if(full_game.scene_events().begin(), full_game.scene_events().end(),
                        [](const auto& event) {
                            return event.kind == AttackEventKind::sid_write;
                        }) == 10);
    for (std::size_t i = 0; i < full_game.scene_events().size(); ++i)
        full_game.apply_scene_event(i);
    for (const auto& sprite : full_game.screen().frame.sprites) test_support::check(sprite.enabled);
    full_game.finish_scene();
    test_support::check(full_game.phase() == FirstTurnPhase::round_tone &&
          full_game.round() == 1 && full_game.turn_mode() == 1 &&
          full_game.random_seed() != result_seed);
    full_game.finish_tone();
    test_support::check(full_game.phase() == FirstTurnPhase::weapon &&
          full_game.first_name() == "BOB" && full_game.second_name() == "ALICE" &&
          full_game.left_score() == 1 && full_game.right_score() == 0);

}

void test_computer_turns_and_shared_input_fifo() {
    // N/COMPUTER/BOB: the first CT=2 decision is MM=1 and falls through
    // BASIC 201 to RAIN without consuming another random value.
    FirstTurn computer(board("COMP"));
    const auto before_ai = computer.random_seed();
    computer.finish_tone();
    test_support::check(computer.phase() == FirstTurnPhase::attack && computer.weapon() == "RAIN");
    test_support::check(computer.computer_decision().has_value() &&
          computer.computer_decision()->attempts == 0);
    test_support::check(computer.charge() == 139.12254202365875 && computer.wind() == -41.0 &&
          computer.random_seed() == before_ai && !computer.waiting_for_input());
    test_support::check(std::any_of(computer.attack_result().events.begin(), computer.attack_result().events.end(),
                      [](const auto& event) { return event.kind == AttackEventKind::sid_write; }));

    // The natural first computer attack leaves G=38. BOB's HAIL 0 then reaches
    // the second left-computer turn at the independently captured MM/AA/EE.
    computer.finish_attack();
    test_support::check(computer.phase() == FirstTurnPhase::round_tone &&
          computer.left_structure() == 43 && computer.right_structure() == 38);
    computer.finish_tone(); test_support::check(computer.waiting_for_weapon());
    computer.type('H'); computer.confirm(); computer.type('0'); computer.confirm();
    test_support::check(computer.phase() == FirstTurnPhase::attack); computer.finish_attack();
    test_support::check(computer.phase() == FirstTurnPhase::round_tone && computer.round() == 2 &&
          computer.cloud() == 4.8979322388768196 && computer.wind() == -49.0);
    const auto before_left_selection = computer.random_seed();
    computer.finish_tone();
    test_support::check(computer.phase() == FirstTurnPhase::attack &&
          computer.computer_decision().has_value() &&
          computer.computer_decision()->attempts >= 1 &&
          computer.random_seed() != before_left_selection);
    test_support::check(std::any_of(computer.attack_result().events.begin(), computer.attack_result().events.end(),
                      [](const auto& event) { return event.kind == AttackEventKind::sid_write; }));
    computer.finish_attack();
    test_support::check(computer.phase() == FirstTurnPhase::round_tone);

    // A computer opponent on the right does not disable the left human's first
    // input, then executes CT=1 automatically on its own turn.
    FirstTurn mixed(board("ALICE", "COMP")); mixed.finish_tone();
    test_support::check(mixed.waiting_for_weapon());
    mixed.type('H'); mixed.confirm(); mixed.type('0'); mixed.confirm(); mixed.finish_attack();
    test_support::check(mixed.phase() == FirstTurnPhase::round_tone && mixed.attacker() == 0);
    mixed.finish_tone();
    test_support::check(mixed.phase() == FirstTurnPhase::attack && mixed.weapon() == "RAIN" &&
          mixed.computer_decision().has_value());
    // Natural turn transitions with a right computer must still obey line46:
    // no new CT assignment in nature; the previous AI reset CT at line76.
    unsigned computer_nature_cases = 0;
    for (unsigned byte = 0; byte < 32 && computer_nature_cases < 2; ++byte) {
        auto seed = BasicRandom::default_seed(); seed.back() = static_cast<std::uint8_t>(byte);
        FirstTurn scenario(board("ALICE", "COMP"), seed);
        for (unsigned transition = 0; transition < 160; ++transition) {
            if (scenario.phase() == FirstTurnPhase::round_tone) {
                const bool is_nature = scenario.turn_mode() == 3;
                scenario.finish_tone();
                if (is_nature) {
                    test_support::check(scenario.phase() == FirstTurnPhase::nature_intro && !scenario.computer_decision());
                    ++computer_nature_cases; break;
                }
            } else if (scenario.waiting_for_weapon()) {
                scenario.type('H'); scenario.confirm(); scenario.type('0'); scenario.confirm();
            } else if (scenario.phase() == FirstTurnPhase::attack) scenario.finish_attack();
            else break;
        }
    }
    test_support::check(computer_nature_cases == 2);

    // Statistics and scene input share one ten-byte FIFO.  H/RETURN was
    // already buffered before statistics; Q/RETURN arrives during the pause
    // and therefore becomes the charge text after H selects the weapon.  A
    // second independent statistics queue would incorrectly quit first and
    // lose the older H bytes.
    FirstTurn fifo(board());
    fifo.type('S'); fifo.confirm();
    fifo.type('H'); fifo.confirm();
    fifo.finish_tone();
    test_support::check(fifo.phase() == FirstTurnPhase::statistics);
    fifo.type('Q'); fifo.confirm();
    fifo.finish_statistics();
    test_support::check(fifo.phase() == FirstTurnPhase::attack && fifo.weapon() == "HAIL" &&
          fifo.charge() == 0.0);

}

void test_computer_match_reaches_result_and_replays() {
    for (unsigned variant = 0; variant < 32; ++variant) {
        auto seed = BasicRandom::default_seed();
        if (variant != 0) seed.back() = static_cast<std::uint8_t>(variant);
        FirstTurn match(board("COMP", "COMP"), seed);
        bool left_attacked = false;
        bool right_attacked = false;
        unsigned computer_attacks = 0;
        unsigned sid_scenes = 0;
        for (unsigned transition = 0;
             transition < 4096 && match.phase() != FirstTurnPhase::result_effect;
             ++transition) {
            if (match.phase() == FirstTurnPhase::round_tone) {
                const auto mode = match.turn_mode();
                const auto attacker = match.attacker();
                match.finish_tone();
                if (mode != 3) {
                    test_support::check(match.phase() == FirstTurnPhase::attack &&
                          match.computer_decision().has_value());
                    if (attacker == 1) left_attacked = true;
                    if (attacker == 0) right_attacked = true;
                    ++computer_attacks;
                }
            } else if (match.phase() == FirstTurnPhase::attack ||
                       match.phase() == FirstTurnPhase::nature_intro) {
                const auto sid_events = std::count_if(
                    match.attack_result().events.begin(), match.attack_result().events.end(),
                    [](const auto& event) { return event.kind == AttackEventKind::sid_write; });
                test_support::check(sid_events > 0);
                ++sid_scenes;
                match.finish_attack();
            } else {
                test_support::check(false);
            }
        }
        test_support::check(match.phase() == FirstTurnPhase::result_effect &&
              left_attacked && right_attacked && computer_attacks > 8 && sid_scenes > 8);
        test_support::check(std::count_if(match.scene_events().begin(), match.scene_events().end(),
                            [](const auto& event) { return event.kind == AttackEventKind::sid_write; }) > 0);
        for (std::size_t i = 0; i < match.scene_events().size(); ++i)
            match.apply_scene_event(i);
        match.finish_scene();
        test_support::check(match.phase() == FirstTurnPhase::result_prompt);

        match.type('Y'); match.confirm();
        test_support::check(match.phase() == FirstTurnPhase::replay_music && match.playing_scene());
        test_support::check(std::count_if(match.scene_events().begin(), match.scene_events().end(),
                            [](const auto& event) { return event.kind == AttackEventKind::sid_write; }) > 0);
        match.finish_scene();
        test_support::check(match.phase() == FirstTurnPhase::replay_board);
        test_support::check(std::count_if(match.scene_events().begin(), match.scene_events().end(),
                            [](const auto& event) { return event.kind == AttackEventKind::sid_write; }) > 0);
        for (std::size_t i = 0; i < match.scene_events().size(); ++i)
            match.apply_scene_event(i);
        match.finish_scene();
        test_support::check(match.phase() == FirstTurnPhase::round_tone && match.round() == 1);
        match.finish_tone();
        test_support::check(match.phase() == FirstTurnPhase::attack &&
              match.computer_decision().has_value());
        test_support::check(std::count_if(match.attack_result().events.begin(), match.attack_result().events.end(),
                            [](const auto& event) { return event.kind == AttackEventKind::sid_write; }) > 0);
    }
}

int main() {
    test_human_turn_and_input();
    test_nature_turn_and_charge_selection();
    test_match_result_and_replay();
    test_computer_turns_and_shared_input_fifo();
    test_computer_match_reaches_result_and_replays();
    std::cout << "Human and computer turns, input retry, all weapons and computer-match replay passed.\n";
}
