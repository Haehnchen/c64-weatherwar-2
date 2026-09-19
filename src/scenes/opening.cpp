#include "scenes/opening.hpp"
#include "basic/basic_input.hpp"
#include "scenes/opening_data.hpp"

#include <cctype>
#include <utility>

namespace weatherwar {

Opening::Opening(std::array<std::uint8_t, 2048> uppercase,
                 std::array<std::uint8_t, 2048> lowercase)
    : uppercase_(std::move(uppercase)), lowercase_(std::move(lowercase)) {
    show(OpeningPage::melody);
}

void Opening::run(std::span<const std::uint16_t> operations) {
    for (const auto operation : operations) {
        if (operation >= 0x100) {
            const auto column = operation - 0x100;
            if (screen_.column > column) screen_.newline();
            // BASIC TAB advances the editor cursor; it does not recolor blanks.
            while (screen_.column < column) screen_.put(29);
        } else {
            screen_.put(static_cast<std::uint8_t>(operation));
        }
    }
}

void Opening::show(OpeningPage page) {
    page_ = page;
    input_.clear();
    boundary_requested_ = false;
    if (page == OpeningPage::melody || page == OpeningPage::question) {
        screen_ = TextScreen{};
        screen_.frame.charset = uppercase_;
        run(opening_data::title_intro);
        if (page == OpeningPage::question) run(opening_data::title_prompt);
    } else {
        // CLEAR retains the current text color; each page then sets yellow/gray.
        screen_.frame.charset = lowercase_;
        run(page == OpeningPage::instructions_one
                ? std::span<const std::uint16_t>(opening_data::instructions_one)
                : std::span<const std::uint16_t>(opening_data::instructions_two));
    }
    prompt_ = screen_;
}

void Opening::finish_melody() {
    if (page_ == OpeningPage::melody) {
        page_ = OpeningPage::question;
        run(opening_data::title_prompt);
        prompt_ = screen_;
        drain_buffered_input();
    }
}

void Opening::buffer_key(char value) {
    // The default C64 keyboard buffer has ten bytes. Text input is already
    // normalized by the same path as a live question; CR/DEL are represented
    // by the private '\r'/'\b' transport bytes for the next scene.
    if (buffered_input_.size() < 10) buffered_input_.push_back(value);
}

void Opening::drain_buffered_input() {
    while (page_ == OpeningPage::question && !boundary_requested_ &&
           !buffered_input_.empty()) {
        const char value = buffered_input_.front();
        buffered_input_.pop_front();
        if (value == '\r') (void)confirm();
        else if (value == '\b') backspace();
        else type(value);
    }
}

void Opening::redraw_input() {
    screen_ = prompt_;
    for (const auto value : input_) screen_.put(static_cast<std::uint8_t>(value));
}

void Opening::type(char value) {
    if (page_ == OpeningPage::melody) {
        const auto upper = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(value)));
        if (upper >= 32 && upper <= 90) buffer_key(static_cast<char>(upper));
        return;
    }
    if (page_ != OpeningPage::question) return;
    const auto upper = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(value)));
    if (upper < 32 || upper > 90) return;
    if (input_.size() >= 79) return;
    input_.push_back(static_cast<char>(upper));
    redraw_input();
}

void Opening::backspace() {
    if (page_ == OpeningPage::melody) {
        buffer_key('\b');
        return;
    }
    if (page_ != OpeningPage::question || input_.empty()) return;
    input_.pop_back();
    redraw_input();
}

bool Opening::confirm() {
    if (page_ == OpeningPage::melody) {
        buffer_key('\r');
        return false;
    }
    if (page_ != OpeningPage::question) return false;
    const auto field = parse_basic_input_field(input_);
    if (field.extra_ignored) {
        for (const unsigned char c : std::string_view("?EXTRA IGNORED\r")) screen_.put(c);
    }
    const char choice = field.value.empty() ? '\0' : field.value.front();
    if (choice == 'Y') {
        show(OpeningPage::instructions_one);
        return false;
    }
    if (choice == 'N') {
        input_.clear();
        boundary_requested_ = true;
        return true;
    }
    // BASIC 13 sets R=1 and repeats the title without replaying the melody.
    input_.clear();
    run(opening_data::title_intro);
    run(opening_data::title_prompt);
    prompt_ = screen_;
    return false;
}

std::string Opening::take_buffered_input() {
    std::string result;
    result.reserve(buffered_input_.size());
    while (!buffered_input_.empty()) {
        result.push_back(buffered_input_.front());
        buffered_input_.pop_front();
    }
    return result;
}

bool Opening::advance(char value) {
    if (page_ == OpeningPage::instructions_one) {
        show(OpeningPage::instructions_two);
    } else if (page_ == OpeningPage::instructions_two) {
        if (value == 'r' || value == 'R') show(OpeningPage::instructions_one);
        else { boundary_requested_ = true; return true; }
    }
    return false;
}

} // namespace weatherwar
