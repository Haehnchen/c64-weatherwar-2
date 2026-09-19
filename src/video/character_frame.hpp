#pragma once

#include <array>
#include <cstdint>

namespace weatherwar {

struct CharacterFrame {
    std::array<std::uint8_t, 1000> screen{};
    std::array<std::uint8_t, 1000> colors{};
    std::array<std::uint8_t, 2048> charset{};
    std::uint8_t background = 0;
    std::uint8_t border = 0;

    struct Sprite {
        std::array<std::uint8_t, 63> data{};
        std::uint16_t x = 0;
        std::uint8_t y = 0;
        bool enabled = false;
        bool expand_x = false;
        bool expand_y = false;
        bool behind_character = false;
        std::uint8_t color = 0;
    };

    std::array<Sprite, 8> sprites{};
};

using PixelBuffer = std::array<std::uint32_t, 320 * 200>;

// Pepto PAL VIC-II palette, represented as 0xFFRRGGBB.
inline constexpr std::array<std::uint32_t, 16> palette{
    0xFF000000, // Black
    0xFFFFFFFF, // White
    0xFF68372B, // Red
    0xFF70A4B2, // Cyan
    0xFF6F3D86, // Purple
    0xFF588D43, // Green
    0xFF352879, // Blue
    0xFFB8C76F, // Yellow
    0xFF6F4F25, // Orange
    0xFF433900, // Brown
    0xFF9A6759, // Light red
    0xFF444444, // Dark gray
    0xFF6C6C6C, // Medium gray
    0xFF9AD284, // Light green
    0xFF6C5EB5, // Light blue
    0xFF959595, // Light gray
};

PixelBuffer render_character_frame(const CharacterFrame& frame);

} // namespace weatherwar
