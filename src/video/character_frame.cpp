#include "character_frame.hpp"

#include <cstddef>

namespace weatherwar {

PixelBuffer render_character_frame(const CharacterFrame& frame) {
    PixelBuffer pixels{};
    std::array<std::uint8_t, 320 * 200> character_foreground{};
    const std::uint32_t background = palette[frame.background & 0x0F];

    for (std::size_t row = 0; row < 25; ++row) {
        for (std::size_t column = 0; column < 40; ++column) {
            const std::size_t cell = row * 40 + column;
            const std::size_t glyph = static_cast<std::size_t>(frame.screen[cell]) * 8;
            const std::uint32_t foreground = palette[frame.colors[cell] & 0x0F];

            for (std::size_t glyph_row = 0; glyph_row < 8; ++glyph_row) {
                const std::uint8_t bits = frame.charset[glyph + glyph_row];
                const std::size_t output_row = row * 8 + glyph_row;
                const std::size_t output_start = output_row * 320 + column * 8;
                for (std::size_t glyph_column = 0; glyph_column < 8; ++glyph_column) {
                    const std::uint8_t mask = static_cast<std::uint8_t>(0x80 >> glyph_column);
                    const bool foreground_pixel = (bits & mask) != 0;
                    pixels[output_start + glyph_column] = foreground_pixel ? foreground : background;
                    character_foreground[output_start + glyph_column] = foreground_pixel ? 1 : 0;
                }
            }
        }
    }

    // VIC sprite 0 has priority over sprite 1, and so on. Claiming an opaque
    // pixel before checking character priority keeps a hidden lower-index
    // sprite from allowing a higher-index sprite to leak through.
    std::array<std::uint8_t, 320 * 200> sprite_owner{};
    sprite_owner.fill(0xFF);
    constexpr int kVisibleX = 24;
    constexpr int kVisibleY = 50;
    for (std::size_t sprite_index = 0; sprite_index < frame.sprites.size(); ++sprite_index) {
        const auto& sprite = frame.sprites[sprite_index];
        if (!sprite.enabled) continue;
        const auto foreground = palette[sprite.color & 0x0F];
        const int x_scale = sprite.expand_x ? 2 : 1;
        const int y_scale = sprite.expand_y ? 2 : 1;
        for (int source_row = 0; source_row < 21; ++source_row) {
            for (int source_byte = 0; source_byte < 3; ++source_byte) {
                const auto bits = sprite.data[static_cast<std::size_t>(source_row * 3 + source_byte)];
                for (int source_bit = 0; source_bit < 8; ++source_bit) {
                    const auto mask = static_cast<std::uint8_t>(0x80 >> source_bit);
                    if ((bits & mask) == 0) continue;
                    const int source_x = source_byte * 8 + source_bit;
                    const int absolute_x = static_cast<int>(sprite.x) + source_x * x_scale;
                    const int absolute_y = static_cast<int>(sprite.y) + source_row * y_scale;
                    for (int dy = 0; dy < y_scale; ++dy) {
                        const int output_y = absolute_y + dy - kVisibleY;
                        if (output_y < 0 || output_y >= 200) continue;
                        for (int dx = 0; dx < x_scale; ++dx) {
                            const int output_x = absolute_x + dx - kVisibleX;
                            if (output_x < 0 || output_x >= 320) continue;
                            const auto output_index = static_cast<std::size_t>(output_y * 320 + output_x);
                            if (sprite_owner[output_index] != 0xFF) continue;
                            sprite_owner[output_index] = static_cast<std::uint8_t>(sprite_index);
                            if (!sprite.behind_character || character_foreground[output_index] == 0) {
                                pixels[output_index] = foreground;
                            }
                        }
                    }
                }
            }
        }
    }
    return pixels;
}

} // namespace weatherwar
