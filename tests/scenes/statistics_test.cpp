#include "scenes/statistics.hpp"
#include "support/check.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {

std::size_t cell(unsigned row, unsigned column) {
    return static_cast<std::size_t>(row) * 40 + column;
}

std::uint8_t screen_code(char value) {
    const auto code = static_cast<unsigned char>(value);
    if (code >= 'A' && code <= 'Z') return static_cast<std::uint8_t>(code - 'A' + 1);
    return code;
}

void test_score_names_positions_and_numeric_spacing() {
    using namespace weatherwar;
    TextScreen initial;
    initial.frame.screen.fill(0x55);
    initial.frame.colors.fill(12);
    initial.frame.charset.fill(0xa5);
    initial.frame.background = 3;
    initial.frame.border = 6;
    initial.column = 7;
    initial.row = 3;
    initial.color = 9;
    initial.reverse = false;
    const auto before = initial;

    const auto screens = render_statistics(initial, "ALICE", "BOB", 12, -7);
    test_support::check(screens.delay_loops == 5000, "BASIC 5000-loop delay must be preserved");
    test_support::check(screens.shown.frame.screen[cell(5, 23)] == screen_code('B') &&
            screens.shown.frame.screen[cell(5, 25)] == screen_code('B'),
            "right name must follow the original HOME/DOWN/UP positioning");
    test_support::check(screens.shown.frame.screen[cell(5, 26)] == '=',
            "right score separator position");
    test_support::check(screens.shown.frame.screen[cell(5, 27)] == 45 &&
            screens.shown.frame.screen[cell(5, 28)] == 55,
            "negative BASIC number must not gain a leading blank");
    test_support::check(screens.shown.frame.screen[cell(5, 29)] == 0x55,
            "numeric PRINT suffix must advance without writing a cell");
    test_support::check(screens.shown.frame.screen[cell(7, 23)] == screen_code('A') &&
            screens.shown.frame.screen[cell(7, 27)] == screen_code('E') &&
            screens.shown.frame.screen[cell(7, 28)] == '=',
            "left name and separator position");
    test_support::check(screens.shown.frame.screen[cell(7, 29)] == 32 &&
            screens.shown.frame.screen[cell(7, 30)] == 49 &&
            screens.shown.frame.screen[cell(7, 31)] == 50,
            "positive BASIC number needs its leading blank");
    test_support::check(screens.shown.frame.colors[cell(5, 23)] == 7 &&
            screens.shown.frame.colors[cell(5, 28)] == 7 &&
            screens.shown.frame.colors[cell(7, 23)] == 7,
            "score display must retain the original yellow color");
    test_support::check(screens.shown.row == 8 && screens.shown.column == 0,
            "the second numeric PRINT must end with a newline");
    test_support::check(screens.shown.color == 7 && !screens.shown.reverse,
            "shown cursor state after BASIC 242");
    test_support::check(initial.frame.screen == before.frame.screen &&
            initial.frame.colors == before.frame.colors &&
            initial.frame.charset == before.frame.charset &&
            initial.frame.background == before.frame.background &&
            initial.frame.border == before.frame.border &&
            initial.column == before.column && initial.row == before.row &&
            initial.color == before.color && initial.reverse == before.reverse,
            "render_statistics must not mutate its input");
}

void test_restore_loop_is_contiguous_and_colored() {
    using namespace weatherwar;
    TextScreen initial;
    initial.frame.screen.fill(0x55);
    initial.frame.colors.fill(12);
    const auto screens = render_statistics(initial, "L", "R", 0, 0);

    // BASIC 243 has one PRINT newline, then 7 semicolon-terminated 24-byte
    // literals. The 168 spaces therefore cover rows 4-7 and eight cells of
    // row 8, with no synthetic final newline.
    for (unsigned row = 4; row <= 7; ++row) {
        for (unsigned column = 0; column < 40; ++column) {
            test_support::check(screens.restored.frame.screen[cell(row, column)] == 32,
                    "restore loop must blank four complete rows");
            test_support::check(screens.restored.frame.colors[cell(row, column)] == 1,
                    "restore loop must use BASIC white color");
        }
    }
    for (unsigned column = 0; column < 8; ++column) {
        test_support::check(screens.restored.frame.screen[cell(8, column)] == 32 &&
                screens.restored.frame.colors[cell(8, column)] == 1,
                "restore loop's final partial row");
    }
    test_support::check(screens.restored.frame.screen[cell(8, 8)] == 0x55 &&
            screens.restored.frame.colors[cell(8, 8)] == 12,
            "restore loop must not write beyond 7x24 spaces");
    test_support::check(screens.restored.row == 8 && screens.restored.column == 8,
            "semicolon-terminated restore loop must not add a newline");
    test_support::check(screens.restored.color == 1 && !screens.restored.reverse,
            "restore loop must retain BASIC white color");

    // `shown` is captured before the delay/restore pass and must retain the
    // score text even though the restore pass subsequently erases it.
    test_support::check(screens.shown.frame.screen[cell(5, 23)] == screen_code('R') &&
            screens.shown.frame.screen[cell(7, 23)] == screen_code('L'),
            "shown and restored must represent separate source states");
}

} // namespace

int main() {
    try {
        test_score_names_positions_and_numeric_spacing();
        test_restore_loop_is_contiguous_and_colored();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "statistics_test: %s\n", error.what());
        return 1;
    }
    return 0;
}
