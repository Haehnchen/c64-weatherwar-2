#include "weather/weather_attack.hpp"

#include "basic/basic_random.hpp"
#include "weather/weapon_data.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace weatherwar {
namespace {

constexpr int kScreenBase = 1024;
constexpr int kScreenEnd = 2023;

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

int memory_address(double value) {
    if (!std::isfinite(value) || value < 0.0 ||
        value > static_cast<double>(std::numeric_limits<std::uint16_t>::max())) {
        throw std::out_of_range("BASIC PEEK/POKE address is outside the C64 address space");
    }
    // All addresses used here are positive. Commodore BASIC discards their
    // fractional part before calling PEEK/POKE.
    return static_cast<int>(value);
}

class Attack {
public:
    explicit Attack(const WeatherAttackInput& input)
        : result_{input.screen, {}, input.ww, input.nn, basic(input.aa), basic(input.charge_a1)}, input_(input) {}

    WeatherAttackResult run() {
        if (input_.weapon == WeatherWeapon::lightning) return run_lightning();

        int cc = 0;
        int ff = 0;
        int o = 0;
        switch (input_.weapon) {
        case WeatherWeapon::hail:
            cc = 58; ff = 4; o = 0;
            delay(135, 1000); // GOSUB 135 from line 83
            break;
        case WeatherWeapon::rain:
            ff = 5; o = 2;
            break;
        case WeatherWeapon::lightning:
            throw std::logic_error("lightning dispatch failure");
        case WeatherWeapon::tornado:
            cc = 102; ff = 7; o = 1;
            tornado_opening();
            break;
        }
        const int tt = ff;

        // BASIC 86. Round stored operands and each arithmetic result at the
        // corresponding BASIC expression/assignment boundary.
        const double wind = input_.weapon == WeatherWeapon::tornado
            ? multiply(1.5, input_.wind_ee) : basic(input_.wind_ee);
        const double e = divide(add(input_.charge_a1, wind), 50.0);
        int dd = 0;
        int gg = 104;
        if (o == 2) {
            cc = e < 0.0 ? 78 : (e > 0.0 ? 77 : 118); // BASIC 87-89
        }

        for (;;) { // BASIC 90, re-entered by line 100 for the erase pass.
            double a = add(1148.0, input_.aa);
            int bb = 0;
            ff = tt;
            const int t = result_.ww;
            int impacts = 0;
            bool returned_early = false;
            double c = 0.0;

            for (int w = 1; w <= 17; ++w) {
                result_.ww = w;
                bool force_last_iteration = false;
                if (t == result_.ww) {
                    force_last_iteration = true;
                    // Keep the drawn extent until the erase pass reaches it.
                    if (dd != 0) result_.ww = 0;
                }

                a = add(a, add(40.0, e));
                c = a;
                // C64 BASIC V2 enters a FOR body before NEXT performs its
                // first limit check. Tornado therefore still draws once when
                // its shrinking FF reaches zero or -1.
                const int z_iterations = input_.weapon == WeatherWeapon::tornado
                    ? std::max(1, ff) : ff;
                for (int z = 1; z <= z_iterations; ++z) {
                    c = add(c, 1.0);
                    int d = peek(c);
                    if (o == 1 && e < 0.0) d = peek(subtract(c, 1.0));

                    if (d == 96) { // BASIC 103/137-139
                        out_of_bounds();
                        force_last_iteration = true;
                        o = 0;
                        returned_early = true;
                        break;
                    }
                    if (d > 102 && dd == 0) {
                        sid(104, 54276, 0);
                        sid(104, 54273, 0);
                        impact(c, o, impacts);
                        ++impacts;
                        if (o != 0) force_last_iteration = true;
                    }
                    if (impacts > 2) force_last_iteration = true;
                    screen_write(106, c, cc);
                }

                bb = 1 - bb;
                if (bb == 0 && input_.weapon == WeatherWeapon::tornado) --ff;
                if (returned_early || force_last_iteration || w == 17) break;

                sid(108, 54276, 0);
                sid(108, 54277, 0);
                sid(108, 54272, 0);
                if (cc == 32) {
                    sid(110, 54277, 9);
                    sid(110, 54276, 33);
                    sid(110, 54273, byte_value(subtract(subtract(cc, 12.0), w)));
                    sid(110, 54272, byte_value(cc));
                } else {
                    sid(109, 54277, 9);
                    sid(109, 54276, 17);
                    sid(109, 54273, byte_value(subtract(cc, multiply(w, 3.0))));
                    sid(109, 54272, byte_value(cc));
                }
            }

            sid(92, 54273, 0);
            sid(92, 54277, 0);
            if (o == 1) tornado_widen(c, e, gg);
            if (result_.nn != 1) delay(135, 1000); // GOSUB 135 from line 99
            if (dd != 0) break;
            dd = 1;
            cc = 32;
            gg = cc;
            (void)bb;
            (void)gg; // GG is retained from the source although H/R do not use 93-98.
        }
        return result_;
    }

private:
    WeatherAttackResult result_;
    const WeatherAttackInput& input_;

    static std::uint8_t byte_value(double value) {
        const double rounded = basic(value);
        if (!std::isfinite(rounded)) throw std::out_of_range("non-finite POKE value");
        // POKE accepts integral values in 0..255; every source expression in
        // this HAIL/RAIN path is integral after BASIC arithmetic.
        const int integer = static_cast<int>(rounded);
        if (integer < 0 || integer > 255 || rounded != integer)
            throw std::out_of_range("BASIC POKE value is outside byte range");
        return static_cast<std::uint8_t>(integer);
    }

    std::size_t screen_index(double address) const {
        const int integer = memory_address(address);
        if (integer < kScreenBase || integer > kScreenEnd)
            throw std::out_of_range("weather trajectory accessed outside text screen RAM");
        return static_cast<std::size_t>(integer - kScreenBase);
    }

    int peek(double address) const {
        return result_.screen.frame.screen.at(screen_index(address));
    }

    void event(AttackEventKind kind, int line, std::uint16_t address,
               std::uint8_t value, int loops = 0,
               std::optional<TextScreen> snapshot = std::nullopt) {
        result_.events.push_back({kind, line, address, value, loops, std::move(snapshot)});
    }

    void screen_write(int line, double address, int value) {
        const int integer = memory_address(address);
        result_.screen.frame.screen.at(screen_index(address)) = static_cast<std::uint8_t>(value);
        event(AttackEventKind::screen_write, line, static_cast<std::uint16_t>(integer),
              static_cast<std::uint8_t>(value));
    }

    void sid(int line, int address, int value) {
        event(AttackEventKind::sid_write, line, static_cast<std::uint16_t>(address),
              byte_value(value));
    }

    void delay(int line, int loops) {
        event(AttackEventKind::delay, line, 0, 0, loops);
    }

    void text_snapshot(int line) {
        event(AttackEventKind::text_screen, line, 0, 0, 0, result_.screen);
    }

    void vic(int line, int address, int value) {
        const auto byte = byte_value(value);
        if (address == 53281) result_.screen.frame.background = byte;
        event(AttackEventKind::vic_write, line, static_cast<std::uint16_t>(address), byte);
    }

    void print(std::span<const std::uint8_t> bytes, int line) {
        result_.screen.print(bytes);
        result_.screen.newline();
        text_snapshot(line);
    }

    void lightning_position() {
        result_.screen.put(19);
        for (int i = 0; i < 4; ++i) result_.screen.put(17);
        const int spaces = static_cast<int>(result_.aa);
        for (int i = 0; i < spaces; ++i) result_.screen.put(29);
        delay(133, 25);
    }

    void lightning_sound() {
        sid(219, 54277, 29);
        sid(219, 54276, 129);
        sid(219, 54273, 6);
        sid(219, 54272, 4);
        for (int v = 1; v <= 20; ++v) {
            vic(220, 53281, 1);
            vic(220, 53272, 22);
            vic(220, 53272, 21);
            vic(220, 53281, 0);
        }
        sid(221, 54276, 0);
        sid(221, 54277, 0);
        sid(221, 54272, 0);
    }

    WeatherAttackResult run_lightning() {
        using namespace weapon_data;
        delay(135, 1000); // GOSUB 135 from line 84
        double a1 = divide(input_.charge_a1, 33.0);
        if (a1 < -4.0) a1 = basic(-4.0);
        if (a1 > 4.0) a1 = basic(4.0);
        if (a1 > 1.0) a1 = subtract(a1, 1.0);
        result_.charge_a1 = a1; // BASIC 118-120 mutates the shared A1 variable.
        result_.aa = add(add(result_.aa, a1), 7.0);
        if (result_.aa > 33.0) result_.aa = basic(33.0);
        if (result_.aa < 6.0) result_.aa = basic(6.0);

        for (int z = 1; z <= 3; ++z) {
            lightning_position(); print(lightning_c, 123);
            vic(123, 53281, 1); vic(123, 53281, 0);
            lightning_position(); print(lightning_d, 123);
        }
        delay(134, 500);
        lightning_position(); print(lightning_c, 124);
        vic(124, 53281, 1); vic(124, 53281, 0);
        lightning_position(); print(lightning_d, 124);
        delay(135, 1000);

        const auto strike = [this](std::span<const std::uint8_t> bolt,
                                   std::span<const std::uint8_t> erase,
                                   int line, double delta) {
            for (int z = 1; z <= 2; ++z) {
                lightning_position(); print(bolt, line);
                vic(line, 53281, 1);
                lightning_sound();
                vic(line, 53281, 0);
                lightning_position(); print(erase, line);
                result_.aa = add(result_.aa, delta);
            }
        };
        if (a1 < 0.0) strike(lightning_e, lightning_f, 127, 1.0);
        else if (a1 > 0.0) strike(lightning_g, lightning_h, 129, -1.0);
        else strike(lightning_i, lightning_j, 131, 0.0);
        return result_;
    }

    void tornado_opening() {
        using namespace weapon_data;
        result_.screen.print(tornado_position);
        for (int v = 1; v <= 20; ++v) result_.screen.print(tornado_cloud);
        result_.screen.newline();
        text_snapshot(85);
    }

    void tornado_tone() {
        sid(225, 54276, 0);
        sid(225, 54277, 0);
        sid(225, 54277, 29);
        sid(225, 54276, 33);
        for (int x = 40; x <= 140; x += 6) sid(226, 54273, 15 + x);
        sid(226, 54276, 0);
        sid(226, 54277, 0);
    }

    void tornado_widen(double c, double e, int gg) {
        if (peek(add(c, 1.0)) == 96 || peek(subtract(c, 1.0)) == 96) return;
        screen_write(95, add(c, 1.0), gg);
        screen_write(95, subtract(c, 1.0), gg);
        if (peek(add(c, 2.0)) == 96 || peek(subtract(c, 2.0)) == 96) return;
        screen_write(96, add(c, 2.0), gg);
        screen_write(96, subtract(c, 2.0), gg);
        if (e >= 0.0) {
            screen_write(96, subtract(c, 38.0), 32);
            screen_write(96, subtract(c, 39.0), 32);
        }
        if (e < 1.0) {
            screen_write(97, subtract(c, 41.0), 32);
            screen_write(97, subtract(c, 42.0), 32);
        }
        if (gg != 32) tornado_tone();
    }

    void impact(double c, int o, int /*prior_impacts*/) {
        for (int x = 1; x <= 2; ++x) {
            screen_write(111, c, 170);
            sid(217, 54277, 8);
            sid(217, 54283, 17);
            sid(217, 54280, 92);
            sid(217, 54279, 169);
            sid(218, 54283, 0);
            sid(218, 54277, 0);
            sid(218, 54272, 0);
            screen_write(111, c, 58);
            sid(217, 54277, 8);
            sid(217, 54283, 17);
            sid(217, 54280, 92);
            sid(217, 54279, 169);
            sid(218, 54283, 0);
            sid(218, 54277, 0);
            sid(218, 54272, 0);
        }
        (void)o;
    }

    void print_number(int value) {
        const std::string rendered = value >= 0
            ? " " + std::to_string(value)
            : std::to_string(value);
        for (const unsigned char ch : rendered) result_.screen.put(ch);
        result_.screen.put(29);
    }

    void out_of_bounds() {
        if (result_.nn == 1) return;

        result_.screen.put(19); // HOME
        for (int i = 0; i < 13; ++i) result_.screen.put(29); // SPC moves the cursor.
        result_.screen.put(18); // RVS ON
        result_.screen.put(5);  // WHITE
        constexpr char message[] = "OUT OF BOUNDS!";
        for (const unsigned char ch : std::string(message)) result_.screen.put(ch);
        result_.screen.newline();
        text_snapshot(138);

        for (int x = 1; x <= 20; ++x) {
            sid(215, 54277, 29);
            sid(215, 54276, 17);
            sid(215, 54273, 17);
            sid(215, 54276, 129);
            sid(216, 54273, 125);
            sid(216, 54276, 0);
            sid(216, 54277, 0);
        }
        // NEXT in 216 falls through to 217; RETURN is only in 218.
        sid(217, 54277, 8);
        sid(217, 54283, 17);
        sid(217, 54280, 92);
        sid(217, 54279, 169);
        sid(218, 54283, 0);
        sid(218, 54277, 0);
        sid(218, 54272, 0);
        delay(138, 2500);

        result_.screen.put(19); // HOME
        for (int i = 0; i < 13; ++i) result_.screen.put(29);
        result_.screen.put(5); // WHITE
        result_.screen.put(0xc0);
        result_.screen.put(0xc0);
        result_.screen.put(32);
        result_.screen.put(0x9e); // YELLOW
        constexpr char round[] = "ROUND";
        for (const unsigned char ch : std::string(round)) result_.screen.put(ch);
        print_number(input_.mm);
        result_.screen.put(5);    // WHITE
        result_.screen.put(0x9d); // LEFT
        result_.screen.put(32);
        result_.screen.put(0xc0);
        result_.screen.put(0xc0);
        result_.screen.put(0xc0);
        result_.screen.newline();
        result_.nn = 1;
        text_snapshot(139);
    }
};

} // namespace

WeatherAttackResult simulate_weather_attack(const WeatherAttackInput& input) {
    return Attack(input).run();
}

} // namespace weatherwar
