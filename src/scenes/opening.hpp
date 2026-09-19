#pragma once

#include "video/text_screen.hpp"
#include <array>
#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <string_view>

namespace weatherwar {

enum class OpeningPage { melody, question, instructions_one, instructions_two };

class Opening {
public:
    Opening(std::array<std::uint8_t, 2048> uppercase,
            std::array<std::uint8_t, 2048> lowercase);
    void show(OpeningPage page);
    void finish_melody();
    void type(char value);
    void backspace();
    // Returns true when the platform should hand off to the name/setup scene.
    bool confirm();
    bool advance(char value);
    // Keys received while the title melody is still running remain in the
    // C64 keyboard buffer. The platform transfers any keys not consumed by
    // the opening prompt to the following setup scene.
    [[nodiscard]] std::string take_buffered_input();
    const TextScreen& screen() const { return screen_; }
    OpeningPage page() const { return page_; }
    const std::string& input() const { return input_; }
    void clear_boundary_notice() { boundary_requested_ = false; }
    bool boundary_requested() const { return boundary_requested_; }

private:
    void run(std::span<const std::uint16_t> operations);
    void redraw_input();
    void buffer_key(char value);
    void drain_buffered_input();
    TextScreen screen_;
    TextScreen prompt_;
    std::array<std::uint8_t, 2048> uppercase_;
    std::array<std::uint8_t, 2048> lowercase_;
    OpeningPage page_ = OpeningPage::melody;
    std::string input_;
    std::deque<char> buffered_input_;
    bool boundary_requested_ = false;
};

} // namespace weatherwar
