#include "scenes/final_score.hpp"
#include "game/result_data.hpp"
#include <string>

namespace weatherwar {
namespace {
void text(TextScreen& screen, std::string_view value) {
    for (unsigned char c : value) screen.put(c);
}
void number(TextScreen& screen, int value) {
    text(screen, (value >= 0 ? " " : "") + std::to_string(value));
    screen.put(29);
}
}

TextScreen render_final_score(const TextScreen& initial, std::string_view left_name,
                             std::string_view right_name, int left_score, int right_score) {
    auto result = initial;
    // BASIC 157 POKE 53269,0. No other sprite register or SID register is reset.
    for (auto& sprite : result.frame.sprites) sprite.enabled = false;
    result.print(result_data::line_157_string_0);
    result.newline();
    result.print(result_data::line_158_string_0);
    text(result, right_name); result.print(result_data::line_158_string_1);
    number(result, right_score); result.newline();
    for (unsigned i = 0; i < 24; ++i) result.put(29);
    result.print(result_data::line_158_string_2);
    text(result, left_name); result.print(result_data::line_158_string_3);
    number(result, left_score); result.print(result_data::line_158_string_4);
    result.newline();
    // ROM $A376: CR/LF READY. CR/LF; LF has no screen movement. The game's
    // final black text colour is retained. The cursor cell keeps its CLEAR
    // fill: KERNAL $EA47-$EA5C draws a visible cursor in the current text
    // colour ($EA1C stores $0286 to Color RAM) and restores the fill colour
    // from $0287 while blinking off, so a captured cursor colour cell is
    // blink-phase-dependent and must not be forced to black here.
    text(result, "\rREADY.\r");
    return result;
}

}
