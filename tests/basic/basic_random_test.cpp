#include "basic/basic_random.hpp"
#include "support/check.hpp"

#include <array>
#include <cmath>
#include <cstdint>

namespace {

using Seed = weatherwar::BasicRandom::Seed;

constexpr std::array<Seed, 12> kViceSeeds{{
    {0x7E, 0x3E, 0x04, 0x7E, 0x4E},
    {0x7C, 0x40, 0x18, 0xC8, 0x69},
    {0x80, 0x53, 0xE7, 0x04, 0x89},
    {0x80, 0x0E, 0x04, 0x0B, 0x97},
    {0x80, 0x65, 0xB1, 0x1D, 0xCA},
    {0x80, 0x12, 0xAA, 0xA3, 0xA3},
    {0x80, 0x56, 0xC1, 0xB3, 0xD0},
    {0x80, 0x6E, 0x65, 0x10, 0x99},
    {0x7E, 0x40, 0xE7, 0x36, 0xA2},
    {0x80, 0x79, 0x12, 0x97, 0x89},
    {0x80, 0x46, 0xC4, 0x5C, 0xB1},
    {0x7F, 0x56, 0x01, 0x7D, 0x19},
}};

void test_vice_seed_transitions() {
    weatherwar::BasicRandom random;
    test_support::check(random.seed() == weatherwar::BasicRandom::default_seed(),
                        "default random seed");
    for (const auto& expected : kViceSeeds) {
        random.next();
        test_support::check(random.seed() == expected, "VICE random seed transition");
    }
}

void test_first_turn_sequence_and_basic_scaling() {
    weatherwar::BasicRandom random;
    const double discarded_line_39 = random.next();
    test_support::check(discarded_line_39 == 0.18556401587557048,
                        "discarded BASIC random value");

    const double cloud_offset = random.next_scaled(26);
    test_support::check(cloud_offset == 1.219364503864199, "scaled cloud offset");
    test_support::check(random.seed() == kViceSeeds[1], "cloud offset random seed");

    const double event_roll = random.next_scaled(100);
    test_support::check(event_roll == 82.77438005805016, "scaled event roll");
    test_support::check(static_cast<int>(event_roll) == 82, "event roll integer part");
    test_support::check(random.seed() == kViceSeeds[2], "event roll random seed");
}

void test_exact_seed_restore() {
    weatherwar::BasicRandom random;
    random.next();
    const Seed checkpoint = random.seed();
    const double expected = random.next();
    random.seed_from_bytes(checkpoint);
    test_support::check(random.next() == expected, "restored random seed");
}

void test_basic_rounding() {
    test_support::check(
        weatherwar::BasicRandom::round_to_basic(1.219364504009718) ==
            1.219364503864199,
        "positive BASIC rounding");
    test_support::check(
        weatherwar::BasicRandom::round_to_basic(-1.219364504009718) ==
            -1.219364503864199,
        "negative BASIC rounding");
    test_support::check(weatherwar::BasicRandom::round_to_basic(0.0) == 0.0,
                        "zero BASIC rounding");
}

} // namespace

int main() {
    test_vice_seed_transitions();
    test_first_turn_sequence_and_basic_scaling();
    test_exact_seed_restore();
    test_basic_rounding();
}
