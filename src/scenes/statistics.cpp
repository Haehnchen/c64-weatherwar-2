#include "scenes/statistics.hpp"

#include "game/result_data.hpp"

#include <string>

namespace weatherwar {
namespace {

void text(TextScreen& screen, std::string_view value) {
    for (const unsigned char character : value) screen.put(character);
}

// Commodore BASIC numeric PRINT emits a leading blank for non-negative
// values. The cursor-right is the PRINT numeric suffix: it advances without
// writing a colored screen cell, matching the other native BASIC renderers.
void number(TextScreen& screen, int value) {
    const std::string rendered = value >= 0
        ? " " + std::to_string(value)
        : std::to_string(value);
    text(screen, rendered);
    screen.put(29);
}

} // namespace

StatisticsScreens render_statistics(const TextScreen& initial,
                                    std::string_view left_name,
                                    std::string_view right_name,
                                    int left_score,
                                    int right_score) {
    StatisticsScreens result{initial, initial, 5000};
    auto& screen = result.shown;

    // BASIC 241: HOME, colour/cursor controls, current right score (Y$/S),
    // then the normal PRINT newline. All quoted bytes come from
    // result_data.hpp.
    screen.print(result_data::line_241_string_0);
    text(screen, right_name);
    screen.print(result_data::line_241_string_1);
    number(screen, right_score);
    screen.newline();

    // BASIC 242 uses SPC(23) for the left score (Z$/RR), represented by
    // cursor-right controls so skipped cells retain their contents/colours.
    screen.print(result_data::line_242_string_0);
    for (unsigned i = 0; i < 23; ++i) screen.put(29);
    text(screen, left_name);
    screen.print(result_data::line_242_string_1);
    number(screen, left_score);
    screen.newline();

    // Preserve the visible statistics screen while the source's FOR/NEXT
    // delay is owned by the audio/timing layer.
    result.restored = screen;

    // BASIC 243's PRINT has its own newline. Lines 244-245 then execute the
    // 7-iteration loop of a semicolon-terminated 24-space literal: no extra
    // newlines are inserted between or after these seven contiguous prints.
    auto& restored = result.restored;
    restored.print(result_data::line_243_string_0);
    restored.newline();
    for (unsigned i = 0; i < 7; ++i) restored.print(result_data::line_244_string_0);

    return result;
}

} // namespace weatherwar
