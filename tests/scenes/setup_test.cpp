#include "scenes/opening.hpp"
#include "scenes/setup.hpp"
#include "support/check.hpp"

#include <iostream>
#include <string>

namespace {

using namespace weatherwar;

TextScreen opening_screen() {
    Opening opening({}, {});
    opening.show(OpeningPage::question);
    opening.type('N');
    return opening.screen();
}

void type_text(Setup& setup, const std::string& text) {
    for (char value : text) setup.type(value);
}

void test_sound_gated_name_flow_and_board() {
    Setup setup(opening_screen(), {}, false);
    test_support::check(setup.playing_tone());
    setup.type('Z');
    test_support::check(setup.input().empty());
    setup.finish_tone();
    test_support::check(setup.phase() == SetupPhase::first_name && setup.input() == "Z");
    setup.backspace();
    test_support::check(setup.input().empty());
    setup.type('A');
    test_support::check(!setup.confirm());
    test_support::check(setup.input().empty());
    test_support::check(!setup.playing_tone());

    type_text(setup, "alicez");
    setup.backspace();
    test_support::check(setup.input() == "ALICE");
    test_support::check(setup.confirm());
    test_support::check(setup.first_name() == "ALICE");
    setup.finish_tone();
    test_support::check(setup.phase() == SetupPhase::second_name);
    type_text(setup, "complicated");
    test_support::check(setup.confirm());
    test_support::check(setup.second_name() == "COMPUTER");
    setup.finish_tone();
    test_support::check(setup.phase() == SetupPhase::board_tone);
    setup.finish_tone();
    test_support::check(setup.phase() == SetupPhase::board);
    test_support::check(!setup.confirm());

    auto replay = setup.screen();
    replay.frame.sprites[0].x = 123;
    replay.frame.sprites[0].data[0] = 0x5a;
    replay.frame.sprites[0].color = 7;
    replay.frame.sprites[0].enabled = false;
    render_match_board(replay, setup.first_name(), setup.second_name());
    test_support::check(replay.frame.screen == setup.screen().frame.screen &&
                        replay.frame.colors == setup.screen().frame.colors);
    test_support::check(replay.frame.sprites[0].x == 123 &&
                        replay.frame.sprites[0].data[0] == 0x5a &&
                        replay.frame.sprites[0].color == 7 &&
                        replay.frame.sprites[0].enabled);

    Setup truncated(opening_screen(), {}, true);
    truncated.finish_tone();
    type_text(truncated, "123456789ABC");
    truncated.confirm();
    test_support::check(truncated.first_name() == "12345678");
}

void test_basic_name_input_rules() {
    Setup parsed(opening_screen(), {}, true);
    parsed.finish_tone();
    type_text(parsed, "  \"alice\"  ");
    test_support::check(parsed.confirm() && parsed.first_name() == "ALICE" &&
                        !parsed.name_extra_ignored());
    parsed.finish_tone();
    type_text(parsed, "comp,BOB");
    test_support::check(parsed.confirm() && parsed.second_name() == "COMPUTER" &&
                        parsed.name_extra_ignored());

    Setup quoted_comma(opening_screen(), {}, true);
    quoted_comma.finish_tone();
    type_text(quoted_comma, "\"a,b\"");
    test_support::check(quoted_comma.confirm() && quoted_comma.first_name() == "A,B");
}

void test_name_input_bounds_and_filtering() {
    Setup bounded(opening_screen(), {}, true);
    bounded.finish_tone();
    type_text(bounded, std::string(79, 'x'));
    test_support::check(bounded.input().size() == 79);
    bounded.type('\n');
    bounded.type(static_cast<char>(127));
    test_support::check(bounded.input().size() == 79);
    test_support::check(bounded.confirm() && bounded.first_name() == "XXXXXXXX");

    Setup filtered(opening_screen(), {}, true);
    filtered.finish_tone();
    filtered.type('\n');
    filtered.type('\r');
    filtered.type('\x1b');
    filtered.type(static_cast<char>(127));
    filtered.type('a');
    test_support::check(filtered.input() == "A");
    test_support::check(!filtered.confirm() && filtered.input().empty());
}

void test_keyboard_buffer_crosses_tones() {
    Setup queued(opening_screen(), {}, false);
    type_text(queued, "ALICE");
    queued.confirm();
    type_text(queued, "BOB");
    queued.confirm();
    queued.finish_tone();
    test_support::check(queued.first_name() == "ALICE" &&
                        queued.phase() == SetupPhase::first_name_tone);
    queued.finish_tone();
    test_support::check(queued.second_name() == "BOB" &&
                        queued.phase() == SetupPhase::second_name_tone);
    queued.finish_tone();
    queued.finish_tone();
    test_support::check(queued.phase() == SetupPhase::board &&
                        queued.take_buffered_input().empty());
}

} // namespace

int main() {
    test_sound_gated_name_flow_and_board();
    test_basic_name_input_rules();
    test_name_input_bounds_and_filtering();
    test_keyboard_buffer_crosses_tones();
    std::cout << "Name rules and four sound-gated setup transitions passed.\n";
}
