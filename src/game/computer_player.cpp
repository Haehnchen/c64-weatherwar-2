#include "game/computer_player.hpp"

#include <cmath>
#include <stdexcept>

namespace weatherwar {
namespace {

double basic(double value) {
    return BasicRandom::round_to_basic(value);
}

double add(double lhs, double rhs) {
    return basic(basic(lhs) + basic(rhs));
}

double subtract(double lhs, double rhs) {
    return basic(basic(lhs) - basic(rhs));
}

double multiply(double lhs, double rhs) {
    return basic(basic(lhs) * basic(rhs));
}

double divide(double lhs, double rhs) {
    return basic(basic(lhs) / basic(rhs));
}

bool occupied(const TextScreen& screen, int address) {
    if (address < 1024 || address > 2023)
        throw std::out_of_range("computer target scan left C64 screen RAM");
    return screen.frame.screen.at(static_cast<std::size_t>(address - 1024)) != 32;
}

WeatherWeapon random_weapon(BasicRandom& random) {
    const int index = static_cast<int>(std::floor(random.next_scaled(3)));
    switch (index) {
    case 0: return WeatherWeapon::hail;
    case 1: return WeatherWeapon::rain;
    case 2: return WeatherWeapon::tornado;
    default: throw std::logic_error("BASIC RND produced an invalid MID$ index");
    }
}

ComputerDecision aim_right(WeatherWeapon weapon, int round, double aa, double wind,
                           const TextScreen& screen, AiMemory& memory, int attempts) {
    ComputerDecision result{weapon, 0.0, wind, attempts, -1, -1};
    if (weapon == WeatherWeapon::lightning) {
        result.charge = aa > 3.0 ? multiply(round, -20.0) : basic(0.0);
        return result;
    }

    // BASIC 192-194, original record $2208-$2276: left-house scan.
    int h = 1668;
    bool found = false;
    for (int y = 0; y <= 4 && !found; ++y) {
        for (int x = 0; x <= 8; ++x) {
            if (!occupied(screen, h + x)) continue;
            memory.p = basic(x);
            memory.pq = divide(subtract(100.0, multiply(y, 5.0)), 25.0);
            memory.qq = basic(x);
            memory.qp = basic(y);
            result.target_x = x;
            result.target_y = y;
            found = true;
            break;
        }
        h += 40;
    }

    result.charge = subtract(
        subtract(multiply(wind, -1.0), multiply(subtract(aa, memory.p), memory.pq)),
        1.0);
    if (weapon == WeatherWeapon::tornado) {
        result.wind = add(divide(wind, 1.5), 5.0);
        return result; // BASIC 196 jumps directly to line 80.
    }
    if (aa < 4.0 && memory.qq > 5.0 && memory.qp > 3.0) {
        result.charge = subtract(subtract(result.charge, memory.qq), aa);
        return result;
    }
    if (aa < 4.0 && memory.qq > 4.0)
        result.charge = subtract(subtract(result.charge, memory.qq), aa);
    return result;
}

ComputerDecision aim_left(WeatherWeapon weapon, int round, double aa, double wind,
                          const TextScreen& screen, AiMemory& memory, int attempts) {
    ComputerDecision result{weapon, 0.0, wind, attempts, -1, -1};
    if (weapon == WeatherWeapon::lightning) {
        result.charge = aa < 23.0 ? multiply(round, 20.0) : basic(0.0);
        return result;
    }

    // BASIC 208-210, original record $2409-$246d: right-house scan.
    int h = 1699;
    bool found = false;
    for (int y = 0; y <= 4 && !found; ++y) {
        for (int x = 0; x <= 8; ++x) {
            if (!occupied(screen, h - x)) continue;
            memory.p = basic(x);
            memory.pq = divide(subtract(100.0, multiply(y, 6.0)), 25.0);
            result.target_x = x;
            result.target_y = y;
            found = true;
            break;
        }
        h += 40;
    }

    result.charge = add(multiply(wind, -1.0),
                        multiply(subtract(subtract(28.0, aa), memory.p), memory.pq));
    if (round == 1) result.charge = subtract(result.charge, 9.0);
    if (weapon == WeatherWeapon::tornado)
        result.wind = subtract(divide(wind, 1.5), 5.0);
    return result;
}

} // namespace

ComputerDecision choose_computer_attack(ComputerSide side, int round, double aa,
                                        double wind, const TextScreen& screen,
                                        BasicRandom& random, AiMemory& memory) {
    aa = basic(aa);
    wind = basic(wind);

    if (side == ComputerSide::left) { // CT=2, BASIC 200-214.
        if (aa > 18.0) {
            memory.lx = add(memory.lx, 1.0);
            if (memory.lx < 3.0 && round < 6)
                return aim_left(WeatherWeapon::lightning, round, aa, wind, screen, memory, 0);
        }
        if (round == 1)
            return aim_left(WeatherWeapon::rain, round, aa, wind, screen, memory, 0);

        for (int pb = 1; pb <= 9; ++pb) {
            if (pb > 8)
                return aim_left(WeatherWeapon::hail, round, aa, wind, screen, memory, pb);

            // Repair line 203's corrupt PEEK$ token as B$, matching the right AI.
            const WeatherWeapon weapon = random_weapon(random);
            const bool tornado_retry = random.next() < basic(0.7);
            if (weapon == WeatherWeapon::tornado && tornado_retry) continue;

            const bool rain_retry = random.next() < basic(0.6);
            if ((weapon == WeatherWeapon::rain && rain_retry) ||
                memory.lx > 0.0 || round > 6)
                continue;
            return aim_left(weapon, round, aa, wind, screen, memory, pb);
        }
        throw std::logic_error("bounded BASIC PB loop did not terminate");
    }

    if (side != ComputerSide::right)
        throw std::invalid_argument("unknown computer side");

    // CT=1, BASIC 184-199.
    if (aa < 8.0) {
        memory.li = add(memory.li, 1.0);
        if (memory.li < 3.0 && round < 7)
            return aim_right(WeatherWeapon::lightning, round, aa, wind, screen, memory, 0);
    }
    if (round == 1)
        return aim_right(WeatherWeapon::rain, round, aa, wind, screen, memory, 0);

    for (int pb = 1; pb <= 9; ++pb) {
        if (pb > 8)
            return aim_right(WeatherWeapon::hail, round, aa, wind, screen, memory, pb);
        const WeatherWeapon weapon = random_weapon(random);

        // Commodore BASIC evaluates both operands of AND. Therefore this RND
        // is consumed for H/R as well as T. A taken THEN skips line 188.
        const bool tornado_retry = random.next() < basic(0.7);
        if (weapon == WeatherWeapon::tornado && tornado_retry) continue;

        // Line 188 likewise evaluates its RND term before the OR terms.
        const bool rain_retry = random.next() < basic(0.6);
        if ((weapon == WeatherWeapon::rain && rain_retry) ||
            memory.li > 0.0 || round > 6)
            continue;
        return aim_right(weapon, round, aa, wind, screen, memory, pb);
    }
    throw std::logic_error("bounded BASIC PB loop did not terminate");
}

} // namespace weatherwar
