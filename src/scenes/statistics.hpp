#pragma once

#include "video/text_screen.hpp"

#include <string_view>

namespace weatherwar {

// BASIC 241-245 result/statistics rendering. `restored` is the screen state
// to use after the original delay, while delay_loops preserves the source
// loop count for the audio/timing owner.
struct StatisticsScreens {
    TextScreen shown;
    TextScreen restored;
    unsigned delay_loops = 5000;
};

// Render the original score display into a copy of `initial`. The input is
// never mutated; SID playback remains the responsibility of the caller while
// the returned source delay is in progress.
[[nodiscard]] StatisticsScreens render_statistics(
    const TextScreen& initial,
    std::string_view left_name,
    std::string_view right_name,
    int left_score,
    int right_score);

} // namespace weatherwar
