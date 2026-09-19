#include "video/character_frame.hpp"
#include "support/check.hpp"

#include <cstddef>
#include <cstdint>

namespace {

void test_foreground_and_background() {
    weatherwar::CharacterFrame frame;
    frame.screen[0] = 1;
    frame.colors[0] = 2;
    frame.background = 6;
    frame.charset[8] = 0xFF;

    const auto pixels = weatherwar::render_character_frame(frame);
    test_support::check(pixels[0] == weatherwar::palette[2], "foreground pixel is wrong");
    test_support::check(pixels[7] == weatherwar::palette[2], "foreground cell width is wrong");
    test_support::check(pixels[8] == weatherwar::palette[6], "background pixel is wrong");
    test_support::check(pixels[320] == weatherwar::palette[6], "background row is wrong");
}

void test_bit_seven_is_leftmost() {
    weatherwar::CharacterFrame frame;
    frame.screen[0] = 2;
    frame.colors[0] = 3;
    frame.background = 4;
    frame.charset[16] = 0x81;

    const auto pixels = weatherwar::render_character_frame(frame);
    test_support::check(pixels[0] == weatherwar::palette[3], "bit seven is not leftmost");
    test_support::check(pixels[1] == weatherwar::palette[4], "bit six is not background");
    test_support::check(pixels[6] == weatherwar::palette[4], "bit one is not background");
    test_support::check(pixels[7] == weatherwar::palette[3], "bit zero is not rightmost");
}

void test_final_cell_is_rendered() {
    weatherwar::CharacterFrame frame;
    frame.screen[999] = 3;
    frame.colors[999] = 5;
    frame.background = 1;
    frame.charset[24] = 0x80;

    const auto pixels = weatherwar::render_character_frame(frame);
    constexpr std::size_t final_pixel = 320 * 200 - 1;
    constexpr std::size_t final_cell_first_pixel = (24 * 8) * 320 + 39 * 8;
    test_support::check(pixels[final_cell_first_pixel] == weatherwar::palette[5],
                        "final cell is not rendered");
    test_support::check(pixels[final_pixel] == weatherwar::palette[1],
                        "final pixel is not rendered");
}

void test_color_indices_are_masked_to_four_bits() {
    weatherwar::CharacterFrame frame;
    frame.screen[0] = 4;
    frame.colors[0] = 0xF9;
    frame.background = 0x16;
    frame.charset[32] = 0x80;

    const auto pixels = weatherwar::render_character_frame(frame);
    test_support::check(pixels[0] == weatherwar::palette[9], "foreground color was not masked");
    test_support::check(pixels[1] == weatherwar::palette[6], "background color was not masked");
}

void test_sprite_expansion_and_vic_coordinates() {
    weatherwar::CharacterFrame frame;
    auto& sprite = frame.sprites[0];
    sprite.enabled = true;
    sprite.x = 24;
    sprite.y = 50;
    sprite.expand_x = true;
    sprite.expand_y = true;
    sprite.color = 2;
    sprite.data[0] = 0x80;

    const auto pixels = weatherwar::render_character_frame(frame);
    test_support::check(pixels[0] == weatherwar::palette[2], "expanded sprite pixel is wrong");
    test_support::check(pixels[1] == weatherwar::palette[2], "expanded sprite width is wrong");
    test_support::check(pixels[320] == weatherwar::palette[2], "expanded sprite height is wrong");
    test_support::check(pixels[321] == weatherwar::palette[2], "expanded sprite area is wrong");
    test_support::check(pixels[2] == weatherwar::palette[0], "sprite coordinate is wrong");
}

void test_sprite_clipping_at_active_area() {
    weatherwar::CharacterFrame frame;
    auto& sprite = frame.sprites[0];
    sprite.enabled = true;
    sprite.x = 22;
    sprite.y = 49;
    sprite.color = 3;
    sprite.data[0] = 0x80;
    sprite.data[3] = 0x20;

    const auto pixels = weatherwar::render_character_frame(frame);
    test_support::check(pixels[0] == weatherwar::palette[3], "left clipped sprite is wrong");
    test_support::check(pixels[320] == weatherwar::palette[0], "top clipped sprite is wrong");

    sprite.x = 342;
    sprite.y = 50;
    sprite.data.fill(0);
    sprite.data[0] = 0xC0;
    const auto right_clipped = weatherwar::render_character_frame(frame);
    test_support::check(right_clipped[319] == weatherwar::palette[3],
                        "right clipped sprite edge is wrong");
    test_support::check(right_clipped[318] == weatherwar::palette[3],
                        "right clipped sprite width is wrong");
}

void test_smaller_sprite_index_wins_even_when_behind_character() {
    weatherwar::CharacterFrame frame;
    frame.screen[0] = 1;
    frame.colors[0] = 1;
    frame.charset[8] = 0x80;
    auto& hidden = frame.sprites[0];
    hidden.enabled = true;
    hidden.x = 24;
    hidden.y = 50;
    hidden.color = 2;
    hidden.behind_character = true;
    hidden.data[0] = 0x80;
    auto& lower_priority = frame.sprites[1];
    lower_priority.enabled = true;
    lower_priority.x = 24;
    lower_priority.y = 50;
    lower_priority.color = 3;
    lower_priority.data[0] = 0x80;

    const auto pixels = weatherwar::render_character_frame(frame);
    test_support::check(pixels[0] == weatherwar::palette[1],
                        "behind-character sprite did not stay hidden");

    hidden.behind_character = false;
    const auto visible = weatherwar::render_character_frame(frame);
    test_support::check(visible[0] == weatherwar::palette[2],
                        "smaller sprite did not win priority");
}

} // namespace

int main() {
    test_foreground_and_background();
    test_bit_seven_is_leftmost();
    test_final_cell_is_rendered();
    test_color_indices_are_masked_to_four_bits();
    test_sprite_expansion_and_vic_coordinates();
    test_sprite_clipping_at_active_area();
    test_smaller_sprite_index_wins_even_when_behind_character();
    return 0;
}
