#include "audio/replay_music.hpp"
#include "audio/music_data.hpp"
#include "basic/basic_random.hpp"
#include <cmath>

namespace weatherwar {
std::vector<AttackEvent> replay_music_events() {
    std::vector<AttackEvent> events;
    const auto sid = [&](int line, unsigned address, unsigned value) {
        events.push_back({AttackEventKind::sid_write, line,
            static_cast<std::uint16_t>(address), static_cast<std::uint8_t>(value), 0, {}});
    };
    const auto reset_voices = [&] {
        sid(227, 54276, 0); sid(227, 54277, 0); sid(227, 54273, 0);
        sid(228, 54283, 0); sid(228, 54284, 0); sid(228, 54280, 0);
    };
    for (const auto note : audio::music_data::notes) {
        reset_voices();
        const auto high1 = BasicRandom::round_to_basic(note.high / BasicRandom::round_to_basic(1.3));
        const auto high2 = BasicRandom::round_to_basic(note.high / 2.0);
        sid(231, 54277, 29); sid(231, 54273, static_cast<unsigned>(high1));
        sid(231, 54272, note.low); sid(231, 54276, 67); sid(231, 54278, 15);
        sid(232, 54284, 29); sid(232, 54280, static_cast<unsigned>(high2)); sid(232, 54283, 17);
        const auto limit = BasicRandom::round_to_basic(note.duration / 17.0);
        for (unsigned t = 1; t <= limit; ++t) sid(233, 54275, t);
    }
    // The sentinel is READ only after both voices have been reset once more.
    // RESTORE then returns; the local DATA traversal starts afresh next call.
    reset_voices();
    return events;
}
}
