#include "game/match_result.hpp"

#include "basic/basic_random.hpp"
#include "game/result_data.hpp"
#include "scenes/setup_data.hpp"

#include <cmath>
#include <string>

namespace weatherwar {
namespace {

void text(TextScreen& screen, std::string_view value) {
    for (const unsigned char character : value) screen.put(character);
}

void spaces(TextScreen& screen, unsigned count) {
    while (count-- != 0) screen.put(29);
}

void number(TextScreen& screen, int value) {
    const std::string rendered = value >= 0
        ? " " + std::to_string(value)
        : std::to_string(value);
    text(screen, rendered);
    // Commodore BASIC's numeric PRINT suffix advances the cursor without
    // writing a space into screen/color RAM.
    screen.put(29);
}

int percentage(double structures) {
    const double factor = BasicRandom::round_to_basic(2.33);
    const double product = BasicRandom::round_to_basic(
        BasicRandom::round_to_basic(structures) * factor);
    return static_cast<int>(std::floor(product));
}

} // namespace

ResultSequence prepare_match_result(const TextScreen& initial,
                                    double left_remaining,
                                    double right_remaining,
                                    std::string_view left_name,
                                    std::string_view right_name) {
    ResultSequence result;
    result.screen = initial;

    auto snapshot = [&](int line) {
        result.events.push_back(
            {AttackEventKind::text_screen, line, 0, 0, 0, result.screen});
    };
    auto sid = [&](int line, unsigned address, unsigned value) {
        result.events.push_back({AttackEventKind::sid_write, line,
            static_cast<std::uint16_t>(address),
            static_cast<std::uint8_t>(value), 0, {}});
    };
    auto background = [&](int line, unsigned value) {
        result.screen.frame.background = static_cast<std::uint8_t>(value);
        result.events.push_back({AttackEventKind::vic_write, line, 53281,
            static_cast<std::uint8_t>(value), 0, {}});
    };

    // BASIC 141: A$; reverse spaces; S$; reverse-off; S$; S$; HOME, followed
    // by PRINT's normal newline.
    result.screen.print(setup_data::line_15_string_0);
    result.screen.print(result_data::line_141_string_0);
    result.screen.print(setup_data::line_16_string_0);
    result.screen.print(result_data::line_141_string_1);
    result.screen.print(setup_data::line_16_string_0);
    result.screen.print(setup_data::line_16_string_0);
    result.screen.print(result_data::line_141_string_2);
    result.screen.newline();
    snapshot(141);

    // BASIC 142 and 225-226: green background around nine complete sweeps.
    background(142, 4);
    for (int repetition = 0; repetition < 9; ++repetition) {
        sid(225, 54276, 0);
        sid(225, 54277, 0);
        sid(225, 54277, 29);
        sid(225, 54276, 33);
        for (int x = 40; x <= 140; x += 6) sid(226, 54273, 15 + x);
        sid(226, 54276, 0);
        sid(226, 54277, 0);
    }
    background(143, 0);

    // BASIC 144-145: center the left name around column 8 with TAB(T).
    double tab = BasicRandom::round_to_basic(
        8.0 - BasicRandom::round_to_basic(
            static_cast<double>(left_name.size()) / 2.0));
    if (tab < 0.0) tab = 0.0;
    result.screen.print(setup_data::line_15_string_0);
    result.screen.print(result_data::line_145_string_0);
    spaces(result.screen, static_cast<unsigned>(tab));
    text(result.screen, left_name);
    result.screen.newline();
    snapshot(145);

    // BASIC 146-147: the quoted LEFT precedes SPC(T), exactly as in source.
    double spc = BasicRandom::round_to_basic(
        33.0 - BasicRandom::round_to_basic(
            static_cast<double>(right_name.size()) / 2.0));
    if (BasicRandom::round_to_basic(
            spc + static_cast<double>(right_name.size())) > 39.0) {
        spc = BasicRandom::round_to_basic(
            40.0 - static_cast<double>(right_name.size()));
    }
    result.screen.print(setup_data::line_15_string_0);
    result.screen.print(result_data::line_147_string_0);
    spaces(result.screen, static_cast<unsigned>(spc));
    text(result.screen, right_name);
    result.screen.newline();
    snapshot(147);

    // BASIC 148-149: INT is applied after the BASIC-rounded multiplication.
    result.screen.print(result_data::line_148_string_0);
    number(result.screen, percentage(left_remaining));
    result.screen.print(result_data::line_148_string_1);
    result.screen.newline();
    snapshot(148);

    result.screen.print(result_data::line_149_string_0);
    spaces(result.screen, 28);
    number(result.screen, percentage(right_remaining));
    result.screen.print(result_data::line_149_string_1);
    result.screen.newline();
    snapshot(149);

    // BASIC 150 leaves the cursor in place for 151 or 152.
    result.screen.print(result_data::line_150_string_0);
    snapshot(150);
    if (left_remaining == right_remaining) {
        result.outcome = MatchOutcome::tie;
        result.screen.print(result_data::line_151_string_0);
        result.screen.newline();
        snapshot(151);
    } else {
        result.screen.print(result_data::line_152_string_0);
        snapshot(152);
        if (left_remaining < right_remaining) {
            result.outcome = MatchOutcome::right_wins;
            result.right_score_delta = 1;
            text(result.screen, right_name);
            result.screen.newline();
            snapshot(152);
        } else {
            result.outcome = MatchOutcome::left_wins;
            result.left_score_delta = 1;
            text(result.screen, left_name);
            result.screen.newline();
            snapshot(153);
        }
    }

    return result;
}

void append_result_prompt(TextScreen& screen) {
    screen.print(setup_data::line_15_string_0);
    screen.print(result_data::line_154_string_0);
    screen.print(setup_data::line_16_string_0);
    screen.print(result_data::line_154_string_1);
    text(screen, "? ");
}

} // namespace weatherwar
