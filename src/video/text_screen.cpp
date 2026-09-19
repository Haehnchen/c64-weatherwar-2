#include "video/text_screen.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace weatherwar {

TextScreen::TextScreen() { clear(); }

void TextScreen::clear() {
    frame.screen.fill(32);
    frame.colors.fill(color);
    column = row = 0;
    reverse = false;
    continuation_.fill(false);
}

void TextScreen::advance_row() {
    if (++row < 25) return;
    std::move(frame.screen.begin() + 40, frame.screen.end(), frame.screen.begin());
    std::move(frame.colors.begin() + 40, frame.colors.end(), frame.colors.begin());
    std::fill(frame.screen.end() - 40, frame.screen.end(), 32);
    std::fill(frame.colors.end() - 40, frame.colors.end(), color);
    std::move(continuation_.begin() + 1, continuation_.end(), continuation_.begin());
    continuation_.back() = false;
    row = 24;
}

void TextScreen::newline() {
    column = 0;
    reverse = false;
    const bool skip_continuation = row < 24 && continuation_[row + 1];
    advance_row();
    if (skip_continuation) advance_row();
}

void TextScreen::wrap() {
    const bool second_row = continuation_[row];
    column = 0;
    advance_row();
    // The KERNAL editor links pairs of physical rows into 80-column lines.
    // Existing links persist when PRINT later moves up and overwrites text.
    continuation_[row] = !second_row;
}

void TextScreen::print(std::span<const std::uint8_t> bytes) {
    for (const auto value : bytes) put(value);
}

void TextScreen::put(std::uint8_t value) {
    constexpr std::array<std::uint8_t, 16> color_codes{
        144, 5, 28, 159, 156, 30, 31, 158, 129, 149, 150, 151, 152, 153, 154, 155};
    const auto found = std::find(color_codes.begin(), color_codes.end(), value);
    if (found != color_codes.end()) {
        color = static_cast<std::uint8_t>(found - color_codes.begin());
        return;
    }
    switch (value) {
    case 13: newline(); return;
    case 17: advance_row(); return;
    case 18: reverse = true; return;
    case 19: column = row = 0; return;
    case 29: if (++column == 40) { column = 0; advance_row(); } return;
    case 145: if (row > 0) --row; return;
    case 146: reverse = false; return;
    case 147: clear(); return;
    case 157:
        if (column > 0) --column;
        else if (row > 0) { --row; column = 39; }
        return;
    default: break;
    }
    if (value < 32 || (value >= 128 && value < 160)) {
        throw std::runtime_error("Unsupported PETSCII control in native opening");
    }
    std::uint8_t code = value;
    if (value >= 64 && value < 96) code = value - 64;
    else if (value >= 96 && value < 128) code = value - 32;
    else if (value >= 160 && value < 192) code = value - 64;
    else if (value >= 192 && value < 255) code = value - 128;
    else if (value == 255) code = 94;
    const auto index = static_cast<unsigned>(row) * 40 + column;
    frame.screen[index] = static_cast<std::uint8_t>(code | (reverse ? 128 : 0));
    frame.colors[index] = color;
    if (++column == 40) wrap();
}

} // namespace weatherwar
