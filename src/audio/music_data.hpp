// Music notes from BASIC DATA, lines 228-240.
// T64 SHA-256: 6d6eba6a5ddc090db8e45de87ce0ca652eaf8a89ac924101857dba84b3577499
// lines.json SHA-256: e0f572be069b69fd7305ba482ccd6ae3e78d86b38f77898014f420a45b45c62a
// Lines 234 and 236 stop at their first unquoted colon; suffix bytes are not DATA.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace weatherwar::audio::music_data {
struct NoteTriplet { std::int16_t high; std::int16_t low; std::int16_t duration; };
inline constexpr std::array<NoteTriplet, 12> notes{
    NoteTriplet{66, 111, 400},
    NoteTriplet{50, 44, 200},
    NoteTriplet{33, 33, 90},
    NoteTriplet{33, 88, 320},
    NoteTriplet{55, 77, 77},
    NoteTriplet{88, 33, 200},
    NoteTriplet{66, 111, 400},
    NoteTriplet{50, 44, 200},
    NoteTriplet{33, 33, 90},
    NoteTriplet{33, 88, 320},
    NoteTriplet{55, 77, 77},
    NoteTriplet{88, 33, 999},
};
inline constexpr std::size_t note_count = 12;
inline constexpr NoteTriplet end_marker{-1, -1, -1};
} // namespace weatherwar::audio::music_data
