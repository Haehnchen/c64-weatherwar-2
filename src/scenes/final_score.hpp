#pragma once

#include "video/text_screen.hpp"
#include <string_view>

namespace weatherwar {

// BASIC 157-158 plus the BASIC END/READY transition. Does not reset the SID:
// the source disables only sprites, and existing audio must keep running.
// The returned frame contains the normal blank cell under the logical cursor;
// cursor blinking/inversion remains a platform concern.
[[nodiscard]] TextScreen render_final_score(
    const TextScreen& initial, std::string_view left_name, std::string_view right_name,
    int left_score, int right_score);

}
