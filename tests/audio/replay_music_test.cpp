#include "audio/replay_music.hpp"
#include "audio/startup_sequence.hpp"
#include "support/check.hpp"
#include <iostream>

void test_replay_events_match_startup_sequence() {
    const auto recorded = weatherwar::audio::builtin_sequence(weatherwar::audio::SequenceId::startup);
    const auto generated = weatherwar::replay_music_events();
    test_support::check(recorded.events.size() == 366 && generated.size() == 365);
    test_support::check(recorded.events.front().reg == 24 && recorded.events.front().value == 15);
    const auto repeated = weatherwar::replay_music_events();
    for (std::size_t i = 0; i < generated.size(); ++i) {
        const auto& actual = generated[i];
        const auto& expected = recorded.events[i + 1];
        test_support::check(actual.kind == weatherwar::AttackEventKind::sid_write &&
                            actual.address == 0xd400 + expected.reg &&
                            actual.value == expected.value && actual.address != 0xd418 &&
                            actual.basic_line >= 227 && actual.basic_line <= 233);
        test_support::check(actual.address == repeated[i].address && actual.value == repeated[i].value);
    }
}

int main() {
    test_replay_events_match_startup_sequence();
    std::cout << "365 replay SID stores match the embedded startup sequence.\n";
}
