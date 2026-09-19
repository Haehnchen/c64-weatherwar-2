#pragma once

#include "video/character_frame.hpp"
#include <cstdint>
#include <span>

namespace weatherwar {

// Bounded native counterpart of the C64 screen editor used by BASIC PRINT.
// Only the control codes needed by the ported opening are accepted.
class TextScreen {
public:
    CharacterFrame frame;
    std::uint8_t column = 0;
    std::uint8_t row = 0;
    std::uint8_t color = 14;
    bool reverse = false;

    TextScreen();
    void clear();
    void newline();
    void print(std::span<const std::uint8_t> bytes);
    void put(std::uint8_t value);

private:
    std::array<bool, 25> continuation_{};
    void wrap();
    void advance_row();
};

} // namespace weatherwar
