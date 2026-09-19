#include "weather/nature_intro.hpp"
#include "scenes/setup_data.hpp"
#include "game/turn_data.hpp"

namespace weatherwar {
WeatherAttackResult simulate_nature_intro(const TextScreen& initial) {
    WeatherAttackResult result;
    result.screen = initial;
    auto snapshot = [&](int line) {
        result.events.push_back({AttackEventKind::text_screen, line, 0, 0, 0, result.screen});
    };
    auto sid = [&](int line, unsigned address, unsigned value) {
        result.events.push_back({AttackEventKind::sid_write, line,
            static_cast<std::uint16_t>(address), static_cast<std::uint8_t>(value), 0, {}});
    };
    auto delay = [&](int line, int count) {
        result.events.push_back({AttackEventKind::delay, line, 0, 0, count, {}});
    };
    auto tone = [&] {
        sid(239, 54276, 0); sid(239, 54277, 0);
        sid(239, 54277, 29); sid(239, 54276, 33);
        for (int x = 160; x >= 1; x -= 5) {
            sid(240, 54273, 66); sid(240, 54273, 33);
        }
        sid(240, 54276, 0); sid(240, 54277, 0);
    };
    for (int z = 1; z <= 2; ++z) {
        result.screen.print(setup_data::line_15_string_0);
        result.screen.print(turn_data::line_56_string_0);
        result.screen.print(setup_data::line_16_string_0);
        result.screen.newline(); snapshot(56);
        tone(); delay(136, 250);
        result.screen.print(setup_data::line_15_string_0);
        // A$ finishes at column zero; TAB(13) therefore advances 13 columns.
        for (unsigned i = 0; i < 13; ++i) result.screen.put(29);
        result.screen.print(turn_data::line_57_string_0);
        result.screen.newline(); snapshot(57);
        tone(); delay(136, 250);
    }
    delay(135, 1000);
    result.screen.print(turn_data::line_58_string_0);
    result.screen.print(setup_data::line_16_string_0);
    result.screen.newline(); snapshot(58);
    return result;
}
}
