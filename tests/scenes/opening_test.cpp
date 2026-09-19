#include "scenes/opening.hpp"
#include "support/check.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <string>

namespace {

weatherwar::Opening make_opening() {
    std::array<std::uint8_t, 2048> uppercase{};
    std::array<std::uint8_t, 2048> lowercase{};
    return weatherwar::Opening(uppercase, lowercase);
}

void test_question_to_instructions_and_repeat() {
    auto opening = make_opening();
    opening.finish_melody();
    test_support::check(opening.page() == weatherwar::OpeningPage::question,
            "melody must finish at the question page");
    opening.type('y');
    test_support::check(opening.input() == "Y", "question input must normalize to uppercase");
    test_support::check(!opening.confirm(), "Y must not request the gameplay boundary");
    test_support::check(opening.page() == weatherwar::OpeningPage::instructions_one,
            "Y must open instructions page one");
    test_support::check(!opening.advance(' '), "page one must advance to page two");
    test_support::check(opening.page() == weatherwar::OpeningPage::instructions_two,
            "page one advance must select page two");
    test_support::check(!opening.advance('R'), "R on page two must remain in the opening");
    test_support::check(opening.page() == weatherwar::OpeningPage::instructions_one,
            "R on page two must repeat page one");
    test_support::check(!opening.boundary_requested(), "R repeat must not request the boundary");
}

void test_invalid_response_redraws_question_without_melody() {
    auto opening = make_opening();
    opening.show(weatherwar::OpeningPage::question);
    const auto prompt = opening.screen().frame.screen;
    const auto prompt_sample = std::find_if(prompt.begin(), prompt.end(), [](std::uint8_t value) {
        return value != 32;
    });
    test_support::check(prompt_sample != prompt.end(), "question prompt must contain visible text");
    const auto sample_index = static_cast<std::size_t>(prompt_sample - prompt.begin());
    opening.finish_melody();
    opening.type('x');
    test_support::check(!opening.confirm(), "invalid response must not request the boundary");
    test_support::check(opening.page() == weatherwar::OpeningPage::question,
            "invalid response must redraw the question page");
    test_support::check(opening.input().empty(), "invalid response must clear the input");
    test_support::check(opening.screen().frame.screen[sample_index] == prompt[sample_index],
            "invalid response must restore the question prompt");
    opening.finish_melody();
    test_support::check(opening.page() == weatherwar::OpeningPage::question,
            "redrawn question must not replay the melody");
}

void test_backspace_removes_last_character() {
    auto opening = make_opening();
    opening.show(weatherwar::OpeningPage::question);
    opening.type('a');
    opening.type('b');
    test_support::check(opening.input() == "AB", "typed characters must be retained");
    opening.backspace();
    test_support::check(opening.input() == "A", "backspace must remove only the last character");
    opening.backspace();
    opening.backspace();
    test_support::check(opening.input().empty(), "backspace on an empty input must be harmless");
}

void test_n_requests_gameplay_boundary() {
    auto opening = make_opening();
    opening.show(weatherwar::OpeningPage::question);
    opening.type('N');
    test_support::check(opening.confirm(), "N must report the gameplay boundary");
    test_support::check(opening.boundary_requested(), "N must set the boundary flag");
    test_support::check(opening.page() == weatherwar::OpeningPage::question,
            "N must leave the opening page state explicit");
    opening.clear_boundary_notice();
    test_support::check(!opening.boundary_requested(), "boundary notice must be clearable");
}

void test_question_prompt_follows_melody() {
    auto melody = make_opening();
    const auto melody_screen = melody.screen().frame.screen;
    melody.finish_melody();
    test_support::check(melody.page() == weatherwar::OpeningPage::question,
            "finishing melody must select question");

    auto question = make_opening();
    question.show(weatherwar::OpeningPage::question);
    const auto difference = std::mismatch(melody_screen.begin(), melody_screen.end(),
                                          question.screen().frame.screen.begin());
    test_support::check(difference.first != melody_screen.end(),
            "melody page must not contain the question prompt yet");
    const auto sample_index = static_cast<std::size_t>(difference.first - melody_screen.begin());
    test_support::check(melody.screen().frame.screen[sample_index] ==
                question.screen().frame.screen[sample_index],
            "finishing melody must add the question prompt");
    test_support::check(melody_screen[sample_index] != question.screen().frame.screen[sample_index],
            "melody page must not contain the question prompt yet");
}

void test_input_is_gated_and_decisions_use_first_character() {
    auto opening = make_opening();
    opening.type('Y');
    test_support::check(opening.input().empty(), "melody must not consume early input");
    opening.finish_melody();
    opening.type('y');
    opening.type('e');
    test_support::check(opening.confirm() == false, "Y-prefixed answer must continue opening");
    test_support::check(opening.page() == weatherwar::OpeningPage::instructions_one,
            "Y-prefixed answer must select instructions");
    opening.type('N');
    test_support::check(opening.page() == weatherwar::OpeningPage::instructions_one,
            "instructions must not consume question input");

    auto quit = make_opening();
    quit.finish_melody();
    quit.type('n');
    quit.type('o');
    test_support::check(quit.confirm(), "N-prefixed answer must request boundary");
}

void test_keyboard_buffer_crosses_melody_boundary() {
    auto opening = make_opening();
    opening.type('N');
    opening.confirm();
    for (const char c : std::string("ALICE")) opening.type(c);
    opening.confirm();
    opening.finish_melody();
    test_support::check(opening.boundary_requested(), "early N+RETURN must reach opening boundary");
    test_support::check(opening.input().empty(), "consumed boundary input must leave no title text");
    test_support::check(opening.take_buffered_input() == "ALICE\r",
            "keyboard bytes after N+RETURN must remain for setup");

    auto capped = make_opening();
    for (int i = 0; i < 12; ++i) capped.type('A');
    capped.finish_melody();
    test_support::check(capped.input() == "AAAAAAAAAA",
            "opening must honor the ten-byte keyboard buffer limit");
}

void test_question_input_has_basic_length_limit_and_recovers() {
    auto opening = make_opening();
    opening.show(weatherwar::OpeningPage::question);
    opening.type('N');
    for (int i = 0; i < 78; ++i) opening.type('A');
    test_support::check(opening.input().size() == 79,
            "question input must accept at most 79 characters");
    opening.type('B');
    test_support::check(opening.input().size() == 79 && opening.input().back() == 'A',
            "question input must ignore characters beyond 79");
    opening.backspace();
    test_support::check(opening.input().size() == 78,
            "backspace must recover from a full question input");
    test_support::check(opening.confirm(),
            "a corrected full-length N response must request the gameplay boundary");
    test_support::check(opening.boundary_requested(),
            "the corrected N response must preserve the boundary request");
}

void test_question_uses_basic_input_field_rules() {
    auto quoted = make_opening();
    quoted.show(weatherwar::OpeningPage::question);
    for (const char c : std::string("  \"y\"  ")) quoted.type(c);
    test_support::check(!quoted.confirm() && quoted.page() == weatherwar::OpeningPage::instructions_one,
            "quoted/trimmed Y must follow BASIC INPUT semantics");

    auto comma = make_opening();
    comma.show(weatherwar::OpeningPage::question);
    for (const char c : std::string(" n,ignored")) comma.type(c);
    test_support::check(comma.confirm() && comma.boundary_requested(),
            "first comma field must decide the N boundary");
}

} // namespace

int main() {
    try {
        test_question_to_instructions_and_repeat();
        test_invalid_response_redraws_question_without_melody();
        test_backspace_removes_last_character();
        test_n_requests_gameplay_boundary();
        test_question_prompt_follows_melody();
        test_input_is_gated_and_decisions_use_first_character();
        test_keyboard_buffer_crosses_melody_boundary();
        test_question_input_has_basic_length_limit_and_recovers();
        test_question_uses_basic_input_field_rules();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "opening_test: %s\n", error.what());
        return 1;
    }
    return 0;
}
