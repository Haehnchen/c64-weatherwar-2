#include "basic/basic_random.hpp"
#include "game/match_result.hpp"
#include "support/check.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <vector>

namespace {

std::size_t cell(unsigned row, unsigned column) {
    return static_cast<std::size_t>(row) * 40 + column;
}

std::uint8_t code(char value, bool reverse = false) {
    auto result = static_cast<std::uint8_t>(value);
    if (result >= 'A' && result <= 'Z') result -= 64;
    return static_cast<std::uint8_t>(result | (reverse ? 0x80 : 0));
}

const weatherwar::TextScreen& snapshot(const weatherwar::AttackEvent& event,
                                       int line) {
    test_support::check(event.kind == weatherwar::AttackEventKind::text_screen,
            "expected text snapshot");
    test_support::check(event.basic_line == line && event.text_screen.has_value(),
            "snapshot source line");
    return *event.text_screen;
}

void test_tone_and_video_event_order() {
    using namespace weatherwar;
    TextScreen initial;
    initial.frame.background = 9;
    initial.frame.border = 6;
    initial.frame.charset.fill(0xa5);
    initial.frame.colors.fill(12);
    initial.column = 7;
    initial.row = 3;
    initial.color = 9;
    initial.frame.sprites[2].enabled = true;
    initial.frame.sprites[2].x = 271;
    const auto before = initial;

    const auto result = prepare_match_result(initial, 10, 20, "ALICE", "BOB");
    test_support::check(result.events.size() == 217, "right-win event count");
    test_support::check(snapshot(result.events[0], 141).frame.background == 9,
            "line 141 precedes green background");
    test_support::check(result.events[1].kind == AttackEventKind::vic_write &&
            result.events[1].basic_line == 142 &&
            result.events[1].address == 53281 && result.events[1].value == 4,
            "line 142 green VIC write");

    std::size_t index = 2;
    for (int repetition = 0; repetition < 9; ++repetition) {
        auto expect_sid = [&](int line, unsigned address, unsigned value) {
            const auto& event = result.events[index++];
            test_support::check(event.kind == AttackEventKind::sid_write &&
                    event.basic_line == line && event.address == address &&
                    event.value == value, "result SID sweep sequence");
        };
        expect_sid(225, 54276, 0);
        expect_sid(225, 54277, 0);
        expect_sid(225, 54277, 29);
        expect_sid(225, 54276, 33);
        for (int x = 40; x <= 140; x += 6) expect_sid(226, 54273, 15 + x);
        expect_sid(226, 54276, 0);
        expect_sid(226, 54277, 0);
    }
    test_support::check(index == 209, "nine sweeps contain exactly 207 SID stores");
    test_support::check(result.events[index].kind == AttackEventKind::vic_write &&
            result.events[index].basic_line == 143 &&
            result.events[index].address == 53281 &&
            result.events[index].value == 0,
            "line 143 black VIC write");

    test_support::check(result.screen.frame.background == 0 &&
            result.screen.frame.border == before.frame.border &&
            result.screen.frame.charset == before.frame.charset,
            "only source background writes alter retained VIC state");
    test_support::check(result.screen.frame.sprites[2].enabled &&
            result.screen.frame.sprites[2].x == 271,
            "result must preserve sprite state");
    test_support::check(initial.frame.background == before.frame.background &&
            initial.frame.colors == before.frame.colors &&
            initial.column == before.column && initial.row == before.row,
            "prepare_match_result must not mutate input");
}

void test_text_positions_colors_and_basic_percentages() {
    using namespace weatherwar;
    TextScreen initial;
    initial.frame.colors.fill(12);
    const auto result = prepare_match_result(initial, 10, 20, "ALICE", "BOB");
    const auto& screen = result.screen;

    for (unsigned i = 0; i < 5; ++i) {
        test_support::check(screen.frame.screen[cell(21, 5 + i)] == code("ALICE"[i], true),
                "left name position/reverse state");
        test_support::check(screen.frame.colors[cell(21, 5 + i)] == 1,
                "left name white");
    }
    for (unsigned i = 0; i < 3; ++i) {
        test_support::check(screen.frame.screen[cell(21, 30 + i)] == code("BOB"[i], true),
                "right name position/reverse state");
        test_support::check(screen.frame.colors[cell(21, 30 + i)] == 1,
                "right name white");
    }

    test_support::check(screen.frame.screen[cell(13, 3)] == 32 &&
            screen.frame.screen[cell(13, 4)] == '2' &&
            screen.frame.screen[cell(13, 5)] == '3' &&
            screen.frame.screen[cell(13, 6)] == '%',
            "left INT(F*2.33) and numeric spacing");
    test_support::check(screen.frame.screen[cell(13, 28)] == 32 &&
            screen.frame.screen[cell(13, 29)] == '4' &&
            screen.frame.screen[cell(13, 30)] == '6' &&
            screen.frame.screen[cell(13, 31)] == '%',
            "right INT(G*2.33) and numeric spacing");
    for (unsigned column : {3u, 4u, 5u, 6u, 28u, 29u, 30u, 31u})
        test_support::check(screen.frame.colors[cell(13, column)] == 1,
                "percentage display white");

    constexpr std::string_view winner = "THE WINNER IS BOB";
    for (unsigned i = 0; i < winner.size(); ++i) {
        test_support::check(screen.frame.screen[cell(10, 9 + i)] == code(winner[i]),
                "winner label/name position");
        test_support::check(screen.frame.colors[cell(10, 9 + i)] == 1,
                "winner display white");
    }

    std::vector<int> snapshot_lines;
    for (const auto& event : result.events) {
        if (event.kind == AttackEventKind::text_screen)
            snapshot_lines.push_back(event.basic_line);
    }
    test_support::check(snapshot_lines == std::vector<int>({141, 145, 147, 148, 149, 150, 152, 152}),
            "one snapshot per executed BASIC PRINT");
    test_support::check(result.screen.row == 11 && result.screen.column == 0,
            "winner PRINT newline cursor");
}

void test_outcomes_and_score_deltas() {
    using namespace weatherwar;
    TextScreen initial;
    const auto right = prepare_match_result(initial, 0, 11, "LEFT", "RIGHT");
    test_support::check(right.outcome == MatchOutcome::right_wins &&
            right.left_score_delta == 0 && right.right_score_delta == 1,
            "right winner and score delta");

    const auto left = prepare_match_result(initial, 7, 0, "LEFT", "RIGHT");
    test_support::check(left.outcome == MatchOutcome::left_wins &&
            left.left_score_delta == 1 && left.right_score_delta == 0,
            "left winner and score delta");
    test_support::check(left.events[left.events.size() - 2].basic_line == 152 &&
            left.events.back().basic_line == 153,
            "left winner crosses lines 152-153");

    const auto tie = prepare_match_result(initial, 0, 0, "LEFT", "RIGHT");
    test_support::check(tie.outcome == MatchOutcome::tie && tie.left_score_delta == 0 &&
            tie.right_score_delta == 0,
            "tie has no score mutation");
    test_support::check(tie.events.back().basic_line == 151,
            "tie terminates at line 151");
    constexpr std::string_view label = "IT'S A TIE!";
    for (unsigned i = 0; i < label.size(); ++i)
        test_support::check(tie.screen.frame.screen[cell(11, 14 + i)] == code(label[i]),
                "tie text position");
}

void test_prompt_is_separate() {
    using namespace weatherwar;
    TextScreen initial;
    auto result = prepare_match_result(initial, 7, 0, "LEFT", "RIGHT");
    const auto before = result.screen;
    append_result_prompt(result.screen);

    test_support::check(before.frame.screen[cell(23, 0)] == 32,
            "result sequence excludes line 154 prompt");
    constexpr std::string_view prompt = "WANT TO PLAY AGAIN (Y/N)";
    for (unsigned i = 0; i < prompt.size(); ++i) {
        test_support::check(result.screen.frame.screen[cell(23, i)] == code(prompt[i]),
                "result prompt position");
        test_support::check(result.screen.frame.colors[cell(23, i)] == 1,
                "result prompt white");
    }
    test_support::check(result.screen.frame.screen[cell(23, 24)] == '?' &&
            result.screen.row == 23 && result.screen.column == 26,
            "INPUT suffix and cursor position");
}

} // namespace

int main() {
    try {
        test_tone_and_video_event_order();
        test_text_positions_colors_and_basic_percentages();
        test_outcomes_and_score_deltas();
        test_prompt_is_separate();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "match_result_test: %s\n", error.what());
        return 1;
    }
    return 0;
}
