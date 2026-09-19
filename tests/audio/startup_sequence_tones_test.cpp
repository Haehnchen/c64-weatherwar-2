#include "audio/sid_chip.hpp"
#include "audio/startup_sequence.hpp"
#include "support/check.hpp"
#include <array>
#include <algorithm>
#include <cmath>

void test_setup_tones() {
    using namespace weatherwar::audio;
    SidChip chip;
    render_sequence(chip, builtin_sequence(SequenceId::startup));
    constexpr std::array<unsigned, 10> registers{4,5,0,5,4,1,0,4,5,0};
    constexpr std::array<unsigned, 10> values{0,0,0,29,17,17,37,0,0,0};
    constexpr std::array ids{SequenceId::entry, SequenceId::first_name, SequenceId::second_name,
                             SequenceId::board, SequenceId::round};
    for (const auto id : ids) {
        const auto sequence = builtin_sequence(id);
        test_support::check(sequence.events.size() == 10 && sequence.events.front().cycle == 0,
                            "wrong setup tone event count/origin");
        for (unsigned i = 0; i < 10; ++i)
            test_support::check(sequence.events[i].reg == registers[i] &&
                                    sequence.events[i].value == values[i],
                                "setup tone differs from BASIC 222-224");
        const auto pcm = render_sequence(chip, sequence);
        test_support::check(std::any_of(pcm.begin(), pcm.end(),
                                        [](float v) { return std::abs(v) > 0.01f; }),
                            "silent setup tone");
    }
}

int main() {
    test_setup_tones();
}
