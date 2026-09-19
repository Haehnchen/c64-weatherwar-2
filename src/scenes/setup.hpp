#pragma once
#include "video/text_screen.hpp"
#include <deque>
#include <string>
#include <string_view>

namespace weatherwar {
// BASIC 26–36, also used by replay. Preserve sprite geometry/data; line 36
// only enables the sprites initialized once by BASIC 260–276.
void render_match_board(TextScreen& screen, std::string_view left_name, std::string_view right_name);
enum class SetupPhase { entry_tone, first_name, first_name_tone, second_name, second_name_tone, board_tone, board };

// BASIC 14, 26–36, 163–166. Audio completion is supplied by the platform;
// game state and PETSCII output do not depend on SDL.
class Setup {
public:
    Setup(TextScreen previous, const std::array<std::uint8_t, 2048>& uppercase, bool from_instructions);
    void finish_tone();
    void type(char value);
    void backspace();
    bool confirm();
    // Keys arriving during a setup tone remain in the C64 keyboard buffer;
    // the platform can transfer leftovers to the next scene after the board.
    [[nodiscard]] std::string take_buffered_input();
    bool waiting_for_name() const;
    bool playing_tone() const;
    SetupPhase phase() const { return phase_; }
    const TextScreen& screen() const { return screen_; }
    const std::string& input() const { return input_; }
    const std::string& first_name() const { return first_; }
    const std::string& second_name() const { return second_; }
    bool name_extra_ignored() const { return name_extra_ignored_; }
private:
    void prompt();
    void redraw();
    void board();
    void buffer_key(char value);
    void drain_buffered_input();
    void text(const std::string& value);
    void spaces(unsigned count);
    TextScreen screen_, prompt_;
    SetupPhase phase_ = SetupPhase::entry_tone;
    std::string input_, first_, second_;
    std::deque<char> buffered_input_;
    bool name_extra_ignored_ = false;
};
}
