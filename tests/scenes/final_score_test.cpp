#include "scenes/final_score.hpp"
#include "support/check.hpp"
#include <iostream>
#include <string_view>

using namespace weatherwar;

namespace {

static bool contains(const TextScreen& screen, std::string_view text) {
    for (unsigned start = 0; start + text.size() <= screen.frame.screen.size(); ++start) {
        bool equal = true;
        for (unsigned i = 0; i < text.size(); ++i) {
            const unsigned char value = text[i];
            const unsigned char code = value >= 64 && value < 96 ? value - 64 : value;
            equal &= screen.frame.screen[start + i] == code;
        }
        if (equal) return true;
    }
    return false;
}
static void row_equals(const TextScreen& screen, unsigned row, std::string_view text) {
    test_support::check(text.size() <= 40, "final score row fits screen");
    for (unsigned column = 0; column < 40; ++column) {
        unsigned char expected = column < text.size() ? text[column] : ' ';
        if (expected >= 64 && expected < 96) expected -= 64;
        test_support::check(screen.frame.screen[row * 40 + column] == expected,
                            "final score row cell");
    }
}

void test_final_score_rendering() {
    TextScreen original;
    original.color = 1;
    original.frame.colors.fill(1);
    original.frame.border = 6;
    original.frame.background = 0;
    original.frame.charset.fill(0xa5);
    for (auto& sprite : original.frame.sprites) {
        sprite.enabled = true; sprite.x = 120; sprite.data.fill(0x55);
    }
    const auto result = render_final_score(original, "ALICE", "BOB", 12, 3);
    test_support::check(contains(result, "THANK YOU FOR PLAYING WEATHERWAR II"),
                        "final score title");
    test_support::check(contains(result, "BOB= 3") && contains(result, "ALICE= 12"),
                        "final score names and scores");
    test_support::check(contains(result, "READY."), "final score prompt");
    row_equals(result, 2, "  THANK YOU FOR PLAYING WEATHERWAR II");
    row_equals(result, 6, "                        BOB= 3");
    row_equals(result, 7, "    THE FINAL SCORE'S :");
    row_equals(result, 8, "                        ALICE= 12");
    row_equals(result, 10, "READY.");
    row_equals(result, 11, "");
    for (unsigned row = 0; row < 25; ++row) {
        if (row != 2 && row != 6 && row != 7 && row != 8 && row != 10)
            row_equals(result, row, "");
    }
    test_support::check(result.color == 0 && result.column == 0 && result.row == 11,
                        "final score cursor state");
    auto expected_colors = original.frame.colors;
    for (unsigned column = 24; column < 30; ++column)
        expected_colors[6 * 40 + column] = 7;
    for (unsigned column = 0; column < 24; ++column)
        expected_colors[7 * 40 + column] = 3;
    for (unsigned column = 24; column < 33; ++column)
        expected_colors[8 * 40 + column] = 7;
    for (unsigned column = 0; column < 6; ++column)
        expected_colors[10 * 40 + column] = 0;
    // The cursor cell keeps its CLEAR fill (tracked colour 1 here): KERNAL
    // $EA47-$EA5C draws a visible cursor in $0286 and restores the fill from
    // $0287 while off, so a captured cursor colour is blink phase, not state.
    expected_colors[11 * 40] = 1;
    test_support::check(result.frame.colors == expected_colors,
                        "final score colors");
    test_support::check(result.frame.border == 6 && result.frame.background == 0,
                        "final score VIC state");
    test_support::check(result.frame.charset == original.frame.charset,
                        "final score charset");
    for (unsigned i = 0; i < result.frame.sprites.size(); ++i) {
        test_support::check(!result.frame.sprites[i].enabled && original.frame.sprites[i].enabled,
                            "final score disables sprites");
        test_support::check(result.frame.sprites[i].data == original.frame.sprites[i].data,
                            "final score preserves sprite data");
        test_support::check(result.frame.sprites[i].x == 120,
                            "final score preserves sprite position");
    }
    test_support::check(!contains(original, "READY."), "final score does not mutate input");
    std::cout << "Final scores, name ownership, sprite disable and retained video state passed.\n";
}

} // namespace

int main() {
    test_final_score_rendering();
}
