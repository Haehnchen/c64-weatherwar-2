#include "game/computer_player.hpp"
#include "support/check.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

using namespace weatherwar;

double b(double value) { return BasicRandom::round_to_basic(value); }
double add(double x, double y) { return b(b(x) + b(y)); }
double sub(double x, double y) { return b(b(x) - b(y)); }
double mul(double x, double y) { return b(b(x) * b(y)); }
double div(double x, double y) { return b(b(x) / b(y)); }

void right_direct_lightning_and_round_one_rain() {
    TextScreen screen;
    BasicRandom random;
    const auto original_seed = random.seed();
    AiMemory memory;
    memory.p = 43;
    auto lightning = choose_computer_attack(
        ComputerSide::right, 2, 7.0, -10.0, screen, random, memory);
    test_support::check(lightning.weapon == WeatherWeapon::lightning && lightning.charge == -40.0,
          "right computer early lightning");
    test_support::check(memory.li == 1.0 && random.seed() == original_seed && lightning.attempts == 0,
          "right lightning state/RNG");

    // With two prior lightning counts, AA<8 increments LI to three, fails
    // LI<3, then BASIC 185 forces round-one RAIN.
    memory.li = 2.0;
    auto rain = choose_computer_attack(
        ComputerSide::right, 1, 7.0, -10.0, screen, random, memory);
    test_support::check(rain.weapon == WeatherWeapon::rain && memory.li == 3.0 &&
          rain.attempts == 0,
          "right computer round-one rain fallback");
}

void right_retry_falls_back_to_hail_with_eager_rng() {
    TextScreen screen;
    AiMemory memory;
    memory.li = 1.0; // Line 188 rejects every candidate after consuming its RND.
    memory.p = 43.0;
    BasicRandom actual;
    BasicRandom expected;
    const auto result = choose_computer_attack(
        ComputerSide::right, 2, 10.0, 0.0, screen, actual, memory);

    for (int pb = 1; pb <= 8; ++pb) {
        const int candidate = static_cast<int>(std::floor(expected.next_scaled(3)));
        const double tornado_gate = expected.next(); // Eager line-187 AND operand.
        if (!(candidate == 2 && tornado_gate < b(0.7)))
            (void)expected.next(); // Eager line-188 AND operand.
    }
    test_support::check(result.weapon == WeatherWeapon::hail && result.attempts == 9,
          "right retry PB fallback");
    test_support::check(actual.seed() == expected.seed(), "right retry eager RNG sequence");
}

void left_direct_choices_preserve_rng() {
    TextScreen screen;
    AiMemory memory;
    memory.p = 43;
    BasicRandom random;
    const auto initial = random.seed();

    const auto rain = choose_computer_attack(
        ComputerSide::left, 1, 18.0, 5.0, screen, random, memory);
    test_support::check(rain.weapon == WeatherWeapon::rain && random.seed() == initial,
          "left round-one direct rain");

    const auto lightning = choose_computer_attack(
        ComputerSide::left, 2, 19.0, 5.0, screen, random, memory);
    test_support::check(lightning.weapon == WeatherWeapon::lightning && lightning.charge == 40.0 &&
          memory.lx == 1.0 && random.seed() == initial,
          "left direct lightning");

}

BasicRandom::Seed accepted_hail_seed() {
    BasicRandom stream;
    for (int offset = 0; offset < 100; ++offset) {
        BasicRandom probe(stream.seed());
        if (static_cast<int>(std::floor(probe.next_scaled(3))) == 0)
            return stream.seed();
        (void)stream.next();
    }
    throw std::runtime_error("could not find deterministic hail seed");
}

void left_round_two_selects_and_aims() {
    TextScreen target;
    // Left attacker scans from address 1699 backwards; y=3,x=4 => 1815.
    target.frame.screen[1815 - 1024] = 104;
    AiMemory memory;
    memory.p = 43.0;
    BasicRandom actual(accepted_hail_seed());
    BasicRandom expected(actual.seed());

    const auto result = choose_computer_attack(
        ComputerSide::left, 2, 10.0, 5.0, target, actual, memory);

    (void)expected.next_scaled(3);
    (void)expected.next();
    (void)expected.next();
    test_support::check(result.weapon == WeatherWeapon::hail && result.attempts == 1,
          "left round-two random weapon");
    test_support::check(result.target_x == 4 && result.target_y == 3 && memory.p == 4.0,
          "left round-two chosen unit target");
    test_support::check(actual.seed() == expected.seed(),
          "left round-two eager RNG sequence");
}

void left_late_round_falls_back_with_eager_rng() {
    TextScreen screen;
    AiMemory memory;
    memory.p = 43.0;
    BasicRandom actual;
    BasicRandom expected;
    const auto result = choose_computer_attack(
        ComputerSide::left, 7, 10.0, 0.0, screen, actual, memory);

    for (int pb = 1; pb <= 8; ++pb) {
        const int candidate = static_cast<int>(std::floor(expected.next_scaled(3)));
        const double tornado_gate = expected.next();
        if (!(candidate == 2 && tornado_gate < b(0.7)))
            (void)expected.next();
    }
    test_support::check(result.weapon == WeatherWeapon::hail && result.attempts == 9,
          "left late-round PB fallback");
    test_support::check(actual.seed() == expected.seed(),
          "left late-round eager RNG sequence");
}

BasicRandom::Seed tornado_seed() {
    BasicRandom stream;
    for (int offset = 0; offset < 100; ++offset) {
        BasicRandom probe(stream.seed());
        if (static_cast<int>(std::floor(probe.next_scaled(3))) == 2 &&
            probe.next() >= b(0.7))
            return stream.seed();
        (void)stream.next();
    }
    throw std::runtime_error("could not find deterministic tornado seed");
}

void both_scans_and_tornado_wind() {
    TextScreen right_target;
    // Left attacker scans from address 1699 backwards; y=2,x=3 => 1776.
    right_target.frame.screen[1776 - 1024] = 104;
    AiMemory left_memory;
    left_memory.p = 43;
    BasicRandom left_random;
    const auto left = choose_computer_attack(
        ComputerSide::left, 1, 10.0, 6.0, right_target, left_random, left_memory);
    test_support::check(left.weapon == WeatherWeapon::rain && left.target_x == 3 && left.target_y == 2,
          "left computer right-house scan coordinates");
    test_support::check(left_memory.p == 3.0 && left_memory.pq == div(sub(100.0, mul(2.0, 6.0)), 25.0),
          "left scan P/PQ");
    const double left_expected = sub(
        add(mul(6.0, -1.0), mul(sub(sub(28.0, 10.0), 3.0), left_memory.pq)), 9.0);
    test_support::check(left.charge == left_expected, "left aim arithmetic order");

    TextScreen left_target;
    // Right attacker scans from address 1668 forwards; y=4,x=6 => 1834.
    left_target.frame.screen[1834 - 1024] = 104;
    AiMemory right_memory;
    right_memory.p = 43;
    BasicRandom right_random(tornado_seed());
    const auto right = choose_computer_attack(
        ComputerSide::right, 2, 10.0, -12.0, left_target, right_random, right_memory);
    test_support::check(right.weapon == WeatherWeapon::tornado &&
          right.target_x == 6 && right.target_y == 4,
          "right computer left-house tornado scan");
    test_support::check(right_memory.p == 6.0 && right_memory.qq == 6.0 && right_memory.qp == 4.0 &&
          right_memory.pq == div(sub(100.0, mul(4.0, 5.0)), 25.0),
          "right scan persistent scalars");
    const double right_expected = sub(
        sub(mul(-12.0, -1.0), mul(sub(10.0, 6.0), right_memory.pq)), 1.0);
    test_support::check(right.charge == right_expected, "right aim arithmetic order");
    test_support::check(right.wind == add(div(-12.0, 1.5), 5.0), "right tornado EE adjustment");
}

void empty_scans_retain_persistent_scalars() {
    TextScreen empty;
    AiMemory right_memory{0, 0, 17, 2.5, 7, 4};
    BasicRandom right_random;
    const auto right = choose_computer_attack(
        ComputerSide::right, 2, 10.0, 0.0, empty, right_random, right_memory);
    test_support::check(right.target_x == -1 && right_memory.p == 17.0 && right_memory.pq == 2.5 &&
          right_memory.qq == 7.0 && right_memory.qp == 4.0,
          "empty right scan changed retained values");

    AiMemory left_memory{0, 0, 19, 3.25, 8, 3};
    BasicRandom left_random;
    const auto left = choose_computer_attack(
        ComputerSide::left, 1, 10.0, 0.0, empty, left_random, left_memory);
    test_support::check(left.target_x == -1 && left_memory.p == 19.0 && left_memory.pq == 3.25 &&
          left_memory.qq == 8.0 && left_memory.qp == 3.0,
          "empty left scan changed retained values");
}

void strict_aim_boundaries_and_invalid_side() {
    TextScreen screen;
    BasicRandom random;
    AiMemory right_memory;
    const auto right_boundary = choose_computer_attack(
        ComputerSide::right, 2, 8.0, 0.0, screen, random, right_memory);
    test_support::check(right_memory.li == 0.0 && right_boundary.weapon != WeatherWeapon::lightning,
          "right AA=8 must not take strict AA<8 lightning branch");

    AiMemory left_memory;
    BasicRandom left_random;
    const auto left_boundary = choose_computer_attack(
        ComputerSide::left, 2, 18.0, 0.0, screen, left_random, left_memory);
    test_support::check(left_boundary.weapon != WeatherWeapon::lightning &&
          left_memory.lx == 0.0 && left_boundary.attempts > 0,
          "left AA=18 must not take strict AA>18 lightning branch");

    bool rejected = false;
    try {
        (void)choose_computer_attack(static_cast<ComputerSide>(99), 1, 0.0, 0.0,
                                     screen, left_random, left_memory);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    test_support::check(rejected, "unknown computer side must be rejected");
}

} // namespace

int main() {
    right_direct_lightning_and_round_one_rain();
    right_retry_falls_back_to_hail_with_eager_rng();
    left_direct_choices_preserve_rng();
    left_round_two_selects_and_aims();
    left_late_round_falls_back_with_eager_rng();
    both_scans_and_tornado_wind();
    empty_scans_retain_persistent_scalars();
    strict_aim_boundaries_and_invalid_side();
    std::cout << "Computer selection, aim, persistence and RNG tests passed.\n";
}
