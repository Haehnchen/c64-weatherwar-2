#pragma once

#include "weather/weather_attack.hpp"

#include <string_view>
#include <vector>

namespace weatherwar {

enum class MatchOutcome {
    tie,
    left_wins,
    right_wins,
};

struct ResultSequence {
    TextScreen screen;
    std::vector<AttackEvent> events;
    MatchOutcome outcome = MatchOutcome::tie;
    int left_score_delta = 0;
    int right_score_delta = 0;
};

// Render BASIC 141-153 from the post-attack board. F/G are the left/right
// structure counts returned by BASIC 113-117; score ownership follows
// Z$/RR (left) and Y$/S (right).
[[nodiscard]] ResultSequence prepare_match_result(
    const TextScreen& initial, double left_remaining, double right_remaining,
    std::string_view left_name, std::string_view right_name);

// Append BASIC 154 through the INPUT prompt, including the ROM's "? " suffix.
// Reading and classifying N$ remains controller-owned.
void append_result_prompt(TextScreen& screen);

} // namespace weatherwar
