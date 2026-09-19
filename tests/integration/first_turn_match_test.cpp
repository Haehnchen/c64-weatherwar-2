#include "game/first_turn.hpp"
#include "scenes/opening.hpp"
#include "support/check.hpp"
#include <array>
#include <iostream>
#include <string>

using namespace weatherwar;
FirstTurn start_match() {
    Opening opening({}, {}); opening.show(OpeningPage::question); opening.type('N');
    Setup setup(opening.screen(), {}, false); setup.finish_tone();
    for (char c : std::string("ALICE")) setup.type(c);
    setup.confirm(); setup.finish_tone();
    for (char c : std::string("BOB")) setup.type(c);
    setup.confirm(); setup.finish_tone(); setup.finish_tone();
    return FirstTurn(setup);
}
void test_left_right_and_tie_match_paths() {
    // These native regression itineraries use normal controls and the default RNG.
    constexpr std::array tie_inputs{
        "L0", "L50", "L0", "T100", "H0", "H-50", "T0", "L-150", "L50",
        "L100", "H-50", "L-50", "R50", "T-100", "T-100", "H-150", "H-150", "H50"};
    for (unsigned scenario = 0; scenario < 3; ++scenario) {
        auto game = start_match();
        unsigned inputs = 0, attacks = 0;
        for (unsigned step = 0; step < 512 && game.phase() != FirstTurnPhase::result_effect; ++step) {
            if (game.phase() == FirstTurnPhase::round_tone) game.finish_tone();
            else if (game.waiting_for_weapon()) {
                const std::string input = scenario == 2 ? tie_inputs.at(inputs) :
                    scenario == 1 && !game.attacker() ? "L150" : "H0";
                game.type(input.front()); game.confirm();
                for (char c : input.substr(1)) game.type(c);
                game.confirm(); ++inputs;
            } else if (game.playing_scene()) {
                if (game.phase() == FirstTurnPhase::attack) ++attacks;
                game.finish_scene();
            } else test_support::check(false, "unexpected match phase");
        }
        test_support::check(game.phase() == FirstTurnPhase::result_effect);
        test_support::check(game.left_score() == 0 && game.right_score() == 0);
        if (scenario == 0) test_support::check(inputs == 46 && attacks == 47 && game.left_structure() == 0 && game.right_structure() == 11 && game.round() == 23);
        if (scenario == 1) test_support::check(inputs == 52 && attacks == 53 && game.left_structure() == 10 && game.right_structure() == 0 && game.round() == 26);
        if (scenario == 2) test_support::check(inputs == tie_inputs.size() && game.left_structure() == 0 && game.right_structure() == 0 && game.round() == 9);
        game.finish_scene();
        test_support::check(game.phase() == FirstTurnPhase::result_prompt);
        if (scenario == 1) {
            // Original final L150 still runs with G already zero. BASIC keeps
            // its residual lightning position and transformed A1 at INPUT154.
            test_support::check(game.cloud() == 10.748179633170366 && game.charge() == 3.0);
        }
        test_support::check(game.left_score() == (scenario == 1 ? 1 : 0) && game.right_score() == (scenario == 0 ? 1 : 0));
        const auto seed = game.random_seed();
        game.type('Y'); game.confirm();
        test_support::check(game.first_name() == "BOB" && game.second_name() == "ALICE" && game.random_seed() == seed);
        test_support::check(game.left_score() == (scenario == 0 ? 1 : 0) && game.right_score() == (scenario == 1 ? 1 : 0));
        game.finish_scene(); game.finish_scene(); game.finish_tone();
        test_support::check(game.waiting_for_weapon() && game.round() == 1 && game.left_structure() == 43 && game.right_structure() == 43);
    }
}

int main() {
    test_left_right_and_tie_match_paths();
    std::cout << "Native input-only left/right/tie games and score-preserving replay passed.\n";
}
