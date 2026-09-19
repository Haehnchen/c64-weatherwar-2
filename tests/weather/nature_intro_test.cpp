#include "weather/nature_intro.hpp"
#include "weather/attack_timing.hpp"
#include "weather/nature_timing_data.hpp"
#include "support/check.hpp"
#include <algorithm>
#include <iostream>
#include <vector>

using namespace weatherwar;
static void sid(const AttackEvent& event, int line, unsigned address, unsigned value) {
    test_support::check(event.kind == AttackEventKind::sid_write);
    test_support::check(event.basic_line == line && event.address == address && event.value == value);
}
static std::size_t tone(const std::vector<AttackEvent>& events, std::size_t index) {
    sid(events[index++], 239, 54276, 0);
    sid(events[index++], 239, 54277, 0);
    sid(events[index++], 239, 54277, 29);
    sid(events[index++], 239, 54276, 33);
    for (int x = 160; x >= 1; x -= 5) {
        (void)x;
        sid(events[index++], 240, 54273, 66);
        sid(events[index++], 240, 54273, 33);
    }
    sid(events[index++], 240, 54276, 0);
    sid(events[index++], 240, 54277, 0);
    return index;
}
static const TextScreen& snapshot(const AttackEvent& event, int line) {
    test_support::check(event.kind == AttackEventKind::text_screen && event.basic_line == line);
    test_support::check(event.text_screen.has_value());
    return *event.text_screen;
}
static void delay(const AttackEvent& event, int line, int count) {
    test_support::check(event.kind == AttackEventKind::delay);
    test_support::check(event.basic_line == line && event.delay_loop_count == count);
}
void test_nature_intro() {
    TextScreen initial;
    initial.frame.background = 4;
    initial.frame.charset.fill(0xa5);
    const auto result = simulate_nature_intro(initial);
    test_support::check(result.events.size() == 290);
    std::size_t index = 0;
    for (int repetition = 0; repetition < 2; ++repetition) {
        const auto& blank = snapshot(result.events[index++], 56);
        for (unsigned column = 0; column < 38; ++column) {
            test_support::check(blank.frame.screen[23 * 40 + column] == 32);
            test_support::check(blank.frame.colors[23 * 40 + column] == 6);
        }
        index = tone(result.events, index);
        delay(result.events[index++], 136, 250);

        const auto& title = snapshot(result.events[index++], 57);
        test_support::check(title.frame.screen[23 * 40 + 13] == (32 | 128));
        constexpr char label[] = "ACT OF NATURE";
        for (unsigned offset = 0; offset < sizeof(label) - 1; ++offset) {
            unsigned char code = static_cast<unsigned char>(label[offset]);
            if (code >= 64 && code < 96) code -= 64;
            test_support::check(title.frame.screen[23 * 40 + 14 + offset] == (code | 128));
            test_support::check(title.frame.colors[23 * 40 + 14 + offset] == 7);
        }
        index = tone(result.events, index);
        delay(result.events[index++], 136, 250);
    }
    delay(result.events[index++], 135, 1000);
    const auto& final = snapshot(result.events[index++], 58);
    test_support::check(index == result.events.size());
    for (unsigned column = 0; column < 38; ++column) {
        test_support::check(final.frame.screen[23 * 40 + column] == 32);
        test_support::check(final.frame.colors[23 * 40 + column] == 6);
    }
    test_support::check(result.events.front().basic_line == 56 && result.events.back().basic_line == 58);
    test_support::check(result.screen.frame.background == 4 && result.screen.frame.charset == initial.frame.charset);
    test_support::check(initial.frame.screen[0] == 32 && initial.color == 14);
    const auto timing = schedule_attack(result);
    test_support::check(timing.nature_sid_calibrated && timing.fallback_intervals == 0);
    test_support::check(timing.interpolated_boundaries == 10 && timing.end_cycle == 5284816);
    test_support::check(std::is_sorted(timing.event_cycles.begin(), timing.event_cycles.end()));
    std::size_t store_index = 0;
    for (std::size_t i = 0; i < result.events.size(); ++i)
        if (result.events[i].kind == AttackEventKind::sid_write)
            test_support::check(timing.event_cycles[i] == nature_timing_data::sid_stores[store_index++].cycle);
    test_support::check(store_index == 280);
    auto changed = result.events;
    changed[1].value ^= 1;
    test_support::check(!schedule_events(changed).nature_sid_calibrated);
    changed = result.events;
    changed[71].delay_loop_count = 251;
    test_support::check(!schedule_events(changed).nature_sid_calibrated);
    changed = result.events;
    changed[0].basic_line = 57;
    test_support::check(!schedule_events(changed).nature_sid_calibrated);
    changed.pop_back();
    test_support::check(!schedule_events(changed).nature_sid_calibrated);
    std::cout << "Nature intro: two flash pairs, four original SID effects and delays passed.\n";
}

int main() {
    test_nature_intro();
}
