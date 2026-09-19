#include "scenes/setup.hpp"
#include "basic/basic_input.hpp"
#include "scenes/setup_data.hpp"
#include <cctype>
#include <utility>

namespace weatherwar {
using namespace setup_data;
Setup::Setup(TextScreen previous, const std::array<std::uint8_t, 2048>& uppercase, bool from_instructions)
    : screen_(std::move(previous)) {
    screen_.frame.charset = uppercase;
    if (from_instructions) { screen_.clear(); screen_.newline(); }
    else { screen_.newline(); for (int i = 0; i < 3; ++i) screen_.put(145); }
}
bool Setup::waiting_for_name() const {
    return phase_ == SetupPhase::first_name || phase_ == SetupPhase::second_name;
}
bool Setup::playing_tone() const { return !waiting_for_name() && phase_ != SetupPhase::board; }
void Setup::text(const std::string& value) { for (unsigned char c : value) screen_.put(c); }
// BASIC ROM $AAF8..$AB3B: screen SPC emits cursor-right ($1D), not
// printable spaces. Reverse/color therefore do not alter skipped cells.
void Setup::spaces(unsigned count) { while (count--) screen_.put(29); }
void Setup::prompt() {
    input_.clear();
    screen_.print(line_163_string_0);
    text(phase_ == SetupPhase::first_name ? " 1 " : " 2 ");
    screen_.print(line_163_string_1);
    text("? "); // BASIC INPUT suffix, separate from the PRINT literal.
    prompt_ = screen_;
}
void Setup::finish_tone() {
    switch (phase_) {
    case SetupPhase::entry_tone: phase_ = SetupPhase::first_name; prompt(); drain_buffered_input(); break;
    case SetupPhase::first_name_tone: phase_ = SetupPhase::second_name; prompt(); drain_buffered_input(); break;
    case SetupPhase::second_name_tone: board(); phase_ = SetupPhase::board_tone; break;
    case SetupPhase::board_tone: phase_ = SetupPhase::board; break;
    default: break;
    }
}
void Setup::buffer_key(char value) {
    if (buffered_input_.size() < 10) buffered_input_.push_back(value);
}
void Setup::drain_buffered_input() {
    while (waiting_for_name() && !buffered_input_.empty()) {
        const char value = buffered_input_.front();
        buffered_input_.pop_front();
        if (value == '\r') (void)confirm();
        else if (value == '\b') backspace();
        else type(value);
    }
}
std::string Setup::take_buffered_input() {
    std::string result;
    result.reserve(buffered_input_.size());
    while (!buffered_input_.empty()) {
        result.push_back(buffered_input_.front());
        buffered_input_.pop_front();
    }
    return result;
}
void Setup::redraw() { screen_ = prompt_; text(input_); }
void Setup::type(char value) {
    if (playing_tone()) {
        const auto c = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(value)));
        if (c >= 32 && c <= 90) buffer_key(static_cast<char>(c));
        return;
    }
    if (!waiting_for_name()) return;
    const auto c = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(value)));
    // Accept printable input and DEL within the original 80-column limit.
    if (c < 32 || c > 90 || input_.size() >= 79) return;
    input_.push_back(static_cast<char>(c)); redraw();
}
void Setup::backspace() {
    if (playing_tone()) {
        buffer_key('\b');
        return;
    }
    if (!waiting_for_name() || input_.empty()) return;
    input_.pop_back(); redraw();
}
bool Setup::confirm() {
    if (playing_tone()) {
        buffer_key('\r');
        return false;
    }
    if (!waiting_for_name()) return false;
    screen_.newline();
    const auto field = parse_basic_input_field(input_);
    name_extra_ignored_ = field.extra_ignored;
    if (field.extra_ignored) text("?EXTRA IGNORED\r");
    if (field.value.size() < 2) {
        for (int i = 0; i < 3; ++i) screen_.put(145);
        prompt(); return false;
    }
    const auto name = field.value.starts_with("COMP") ? "COMPUTER" : field.value.substr(0, 8);
    if (phase_ == SetupPhase::first_name) { first_ = name; phase_ = SetupPhase::first_name_tone; }
    else { second_ = name; phase_ = SetupPhase::second_name_tone; }
    return true;
}
void render_match_board(TextScreen& screen_, std::string_view first_, std::string_view second_) {
    const auto spaces = [&](unsigned count) { while (count--) screen_.put(29); };
    const auto text = [&](std::string_view value) { for (unsigned char c : value) screen_.put(c); };
    screen_.print(line_26_string_0);
    for (int i = 0; i < 34; ++i) screen_.print(line_26_string_1);
    screen_.print(line_26_string_2); screen_.newline();
    for (int i = 0; i < 20; ++i) {
        screen_.print(line_28_string_0); spaces(38); screen_.print(line_28_string_1);
    }
    spaces(4); screen_.print(line_29_string_0); spaces(16); screen_.print(line_29_string_1); screen_.newline();
    screen_.print(line_30_string_0); spaces(16); screen_.print(line_30_string_1); screen_.newline();
    screen_.print(line_31_string_0); spaces(14); screen_.print(line_31_string_1); screen_.newline();
    screen_.print(line_32_string_0); spaces(14); screen_.print(line_32_string_1); screen_.newline();
    screen_.print(line_33_string_0); spaces(14); screen_.print(line_33_string_1); screen_.newline();
    screen_.print(line_34_string_0); spaces(static_cast<unsigned>(4.5 + (8 - first_.size()) / 2.0)); text(first_); screen_.newline();
    screen_.print(line_35_string_0); spaces(static_cast<unsigned>(28 + (8 - second_.size()) / 2.0)); text(second_); screen_.newline();
    screen_.print(line_15_string_0);
    for (int i = 0; i < 40; ++i) screen_.print(line_36_string_0);
    screen_.newline();
    for (auto& sprite : screen_.frame.sprites) sprite.enabled = true;
}

void Setup::board() {
    // BASIC 260–276 initializes all eight sprites; line 36 enables them.
    for (unsigned i = 0; i < 8; ++i) {
        auto& sprite = screen_.frame.sprites[i];
        sprite.data = sprite_data;
        sprite.x = sprite_positions[2*i] + ((sprite_positions[16] & (1u << i)) ? 256 : 0);
        sprite.y = sprite_positions[2*i+1];
        sprite.color = sprite_colors[i];
        sprite.expand_x = (137 & (1u << i)) != 0;
        sprite.expand_y = (199 & (1u << i)) != 0;
        sprite.behind_character = (120 & (1u << i)) != 0;
        sprite.enabled = true;
    }
    render_match_board(screen_, first_, second_);
}
}
