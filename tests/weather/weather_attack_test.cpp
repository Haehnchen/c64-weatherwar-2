#include "weather/weather_attack.hpp"
#include "support/check.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

using weatherwar::AttackEvent;
using weatherwar::AttackEventKind;
using weatherwar::WeatherAttackInput;
using weatherwar::WeatherWeapon;

std::vector<const AttackEvent*> events_of(const std::vector<AttackEvent>& events,
                                          AttackEventKind kind) {
    std::vector<const AttackEvent*> selected;
    for (const auto& event : events) if (event.kind == kind) selected.push_back(&event);
    return selected;
}

void trajectory_and_cleanup() {
    WeatherAttackInput input;
    input.weapon = WeatherWeapon::hail;
    input.aa = input.wind_ee = input.charge_a1 = 0;
    input.screen.frame.screen[700] = 42; // Not touched by the trajectory.
    input.screen.frame.colors[165] = 7;

    const auto result = weatherwar::simulate_weather_attack(input);
    test_support::check(result.ww == 0 && result.nn == 0, "normal attack WW/NN result");
    test_support::check(result.screen.frame.screen[700] == 42, "write outside trajectory");
    test_support::check(result.screen.frame.colors == input.screen.frame.colors, "attack changed color RAM");

    const auto writes = events_of(result.events, AttackEventKind::screen_write);
    test_support::check(writes.size() == 17U * 4U * 2U, "hail draw/erase write count");
    test_support::check(writes[0]->address == 1189 && writes[0]->value == 58 &&
          writes[0]->basic_line == 106, "first hail trajectory cell");
    test_support::check(writes[3]->address == 1192 && writes[4]->address == 1229,
          "hail trajectory progression");
    test_support::check(writes[68]->address == 1189 && writes[68]->value == 32,
          "erase pass did not restart trajectory");
    std::array<bool, 1000> touched{};
    for (const auto* write : writes) touched.at(write->address - 1024) = true;
    for (std::size_t i = 0; i < touched.size(); ++i)
        if (!touched[i])
            test_support::check(result.screen.frame.screen[i] == input.screen.frame.screen[i],
                  "screen byte outside trajectory changed");
    for (const auto* write : writes)
        test_support::check(result.screen.frame.screen.at(write->address - 1024) == 32,
              "trajectory was not erased");

    const auto delays = events_of(result.events, AttackEventKind::delay);
    test_support::check(delays.size() == 3 && delays[0]->basic_line == 135 &&
          delays[0]->delay_loop_count == 1000 && delays[1]->basic_line == 135 &&
          delays[1]->delay_loop_count == 1000 && delays[2]->basic_line == 135 &&
          delays[2]->delay_loop_count == 1000, "hail delay provenance");

    const auto sid = events_of(result.events, AttackEventKind::sid_write);
    test_support::check(sid.size() == 16U * 7U * 2U + 4U, "normal hail SID write count");
    test_support::check(sid[0]->basic_line == 108 && sid[0]->address == 54276 && sid[0]->value == 0,
          "line 108 SID order");
    test_support::check(sid[3]->basic_line == 109 && sid[3]->address == 54277 && sid[3]->value == 9,
          "line 109 SID order");
    test_support::check(sid[6]->address == 54272 && sid[6]->value == 58,
          "line 109 final SID write");
}

void rain_erases_only_drawn_cells() {
    WeatherAttackInput input;
    input.weapon = WeatherWeapon::rain;
    input.charge_a1 = -50;
    input.ww = 3;
    input.screen.frame.screen[1500 - 1024] = 104;
    const auto result = weatherwar::simulate_weather_attack(input);
    const auto writes = events_of(result.events, AttackEventKind::screen_write);
    test_support::check(writes.size() == 3U * 5U * 2U, "erase exceeded drawn path");
    test_support::check(writes.front()->value == 78, "negative rain direction glyph");
    test_support::check(result.ww == 0, "erase did not clear the stop point");
    test_support::check(result.screen.frame.screen[1500 - 1024] == 104,
          "erase damaged structure beyond the drawn path");
}

void impact_audio_is_interleaved() {
    WeatherAttackInput input;
    input.weapon = WeatherWeapon::hail;
    input.screen.frame.screen[1189 - 1024] = 104;
    const auto result = weatherwar::simulate_weather_attack(input);
    auto first_flash = std::find_if(result.events.begin(), result.events.end(), [](const auto& event) {
        return event.kind == AttackEventKind::screen_write && event.basic_line == 111 &&
               event.value == 170;
    });
    test_support::check(first_flash != result.events.end(), "missing impact flash");
    test_support::check(first_flash != result.events.begin(), "missing impact preamble");
    test_support::check((first_flash - 2)->kind == AttackEventKind::sid_write &&
          (first_flash - 2)->basic_line == 104 && (first_flash - 2)->address == 54276,
          "impact preamble order");
    test_support::check((first_flash + 1)->kind == AttackEventKind::sid_write &&
          (first_flash + 1)->basic_line == 217 && (first_flash + 1)->address == 54277,
          "impact audio not adjacent to graphics");
    test_support::check((first_flash + 8)->kind == AttackEventKind::screen_write &&
          (first_flash + 8)->value == 58,
          "impact restore not adjacent to first tone");
}

void out_of_bounds_text_and_audio() {
    WeatherAttackInput input;
    input.weapon = WeatherWeapon::rain;
    input.mm = 12;
    input.screen.frame.screen[1189 - 1024] = 96;
    const auto result = weatherwar::simulate_weather_attack(input);
    test_support::check(result.nn == 1 && result.ww == 0, "out-of-bounds state");
    const auto snapshots = events_of(result.events, AttackEventKind::text_screen);
    test_support::check(snapshots.size() == 2 && snapshots[0]->basic_line == 138 &&
          snapshots[1]->basic_line == 139, "text snapshot provenance");
    test_support::check(snapshots[0]->text_screen.has_value() && snapshots[1]->text_screen.has_value(),
          "missing text snapshots");
    // OUT OF BOUNDS! begins at HOME+13, in reverse video.
    test_support::check(snapshots[0]->text_screen->frame.screen[13] == (15 | 128) &&
          snapshots[0]->text_screen->frame.colors[13] == 1,
          "out-of-bounds PRINT rendering");

    const auto delays = events_of(result.events, AttackEventKind::delay);
    test_support::check(delays.size() == 1 && delays[0]->basic_line == 138 &&
          delays[0]->delay_loop_count == 2500, "NN skip-delay behavior");
    const auto sid = events_of(result.events, AttackEventKind::sid_write);
    test_support::check(sid.size() == 20U * 7U + 7U + 4U, "out-of-bounds SID write count including 216 fallthrough");
    test_support::check(sid[140]->basic_line == 217 && sid[146]->basic_line == 218,
          "out-of-bounds omitted the fallthrough into impact audio");
    test_support::check(sid[0]->basic_line == 215 && sid[0]->address == 54277 && sid[0]->value == 29,
          "out-of-bounds audio order");
}

void rejects_bad_bounds() {
    WeatherAttackInput outside;
    outside.aa = 1000;
    bool rejected = false;
    try { (void)weatherwar::simulate_weather_attack(outside); }
    catch (const std::out_of_range&) { rejected = true; }
    test_support::check(rejected, "out-of-screen trajectory was not bounded");

    outside.aa = -1000;
    rejected = false;
    try { (void)weatherwar::simulate_weather_attack(outside); }
    catch (const std::out_of_range&) { rejected = true; }
    test_support::check(rejected, "negative out-of-screen trajectory was not bounded");
}

void rejects_nonfinite_numeric_inputs() {
    WeatherAttackInput input;
    input.aa = std::numeric_limits<double>::quiet_NaN();
    bool rejected = false;
    try { (void)weatherwar::simulate_weather_attack(input); }
    catch (const std::invalid_argument&) { rejected = true; }
    test_support::check(rejected, "NaN AA was accepted");

    input.aa = 0.0;
    input.wind_ee = std::numeric_limits<double>::infinity();
    rejected = false;
    try { (void)weatherwar::simulate_weather_attack(input); }
    catch (const std::invalid_argument&) { rejected = true; }
    test_support::check(rejected, "infinite wind was accepted");

    input.wind_ee = 0.0;
    input.charge_a1 = -std::numeric_limits<double>::infinity();
    rejected = false;
    try { (void)weatherwar::simulate_weather_attack(input); }
    catch (const std::invalid_argument&) { rejected = true; }
    test_support::check(rejected, "infinite charge was accepted");
}

void lightning_zero_charge_vic_sid_and_text() {
    WeatherAttackInput input;
    input.weapon = WeatherWeapon::lightning;
    input.ww = 9;
    input.nn = 1;
    const auto result = weatherwar::simulate_weather_attack(input);
    test_support::check(result.ww == 9 && result.nn == 1, "lightning changed WW/NN");
    test_support::check(result.aa == 7.0, "zero-charge lightning AA");
    test_support::check(result.screen.frame.background == 0, "lightning left background flashed");

    const auto text = events_of(result.events, AttackEventKind::text_screen);
    test_support::check(text.size() == 12, "lightning PRINT snapshot count");
    for (const auto* event : text)
        test_support::check(event->text_screen.has_value() && event->text_screen->frame.background == 0,
              "lightning snapshot background");

    const auto delays = events_of(result.events, AttackEventKind::delay);
    test_support::check(delays.size() == 15, "lightning delay count");
    test_support::check(std::count_if(delays.begin(), delays.end(), [](const auto* event) {
        return event->basic_line == 133 && event->delay_loop_count == 25;
    }) == 12, "lightning positioning delays");

    const auto sid = events_of(result.events, AttackEventKind::sid_write);
    test_support::check(sid.size() == 14, "lightning SID count");
    test_support::check(sid[0]->basic_line == 219 && sid[0]->address == 54277 && sid[0]->value == 29 &&
          sid[6]->basic_line == 221 && sid[6]->address == 54272 && sid[6]->value == 0,
          "lightning SID order");

    const auto vic = events_of(result.events, AttackEventKind::vic_write);
    test_support::check(vic.size() == 172, "lightning VIC write count");
    test_support::check(vic[8]->basic_line == 131 && vic[8]->address == 53281 && vic[8]->value == 1,
          "lightning strike background onset");
    test_support::check(vic[9]->basic_line == 220 && vic[9]->address == 53281 && vic[9]->value == 1 &&
          vic[10]->address == 53272 && vic[10]->value == 22 &&
          vic[11]->address == 53272 && vic[11]->value == 21 &&
          vic[12]->address == 53281 && vic[12]->value == 0,
          "lightning VIC flash order");
}

void lightning_direction_and_basic_arithmetic() {
    WeatherAttackInput left;
    left.weapon = WeatherWeapon::lightning;
    left.aa = 10;
    left.charge_a1 = -132;
    const auto left_result = weatherwar::simulate_weather_attack(left);
    test_support::check(left_result.aa == 15.0, "negative lightning residual AA");

    WeatherAttackInput right;
    right.weapon = WeatherWeapon::lightning;
    right.aa = 20;
    right.charge_a1 = 132;
    const auto right_result = weatherwar::simulate_weather_attack(right);
    test_support::check(right_result.aa == 28.0, "positive lightning residual AA");
    const auto text = events_of(right_result.events, AttackEventKind::text_screen);
    test_support::check(text[text.size() - 2]->basic_line == 129 &&
          text.back()->basic_line == 129, "positive lightning string branch");
}

void tornado_shrinks_widens_and_sweeps() {
    WeatherAttackInput input;
    input.weapon = WeatherWeapon::tornado;
    const auto result = weatherwar::simulate_weather_attack(input);
    test_support::check(result.ww == 0 && result.nn == 0 && result.aa == 0.0,
          "tornado result state");
    const auto text = events_of(result.events, AttackEventKind::text_screen);
    test_support::check(text.size() == 1 && text[0]->basic_line == 85,
          "tornado opening PRINT");

    const auto writes = events_of(result.events, AttackEventKind::screen_write);
    test_support::check(writes.size() == 134, "tornado shrinking/widening write count");
    test_support::check(writes[0]->address == 1189 && writes[0]->value == 102 &&
          writes[6]->address == 1195 && writes[7]->address == 1229,
          "tornado initial FF=7 trajectory");
    // FF has reached zero, but C64 BASIC executes the FOR body once before
    // NEXT checks its limit.
    test_support::check(writes[56]->address == 1749 && writes[57]->address == 1789 &&
          writes[58]->address == 1829,
          "tornado zero/negative FF iterations");
    test_support::check(writes[59]->basic_line == 95 && writes[59]->address == 1830 &&
          writes[60]->address == 1828 && writes[61]->basic_line == 96,
          "tornado residual-C widening order");

    const auto sid = events_of(result.events, AttackEventKind::sid_write);
    test_support::check(sid.size() == 251, "tornado SID write count");
    auto sweep = std::find_if(sid.begin(), sid.end(), [](const auto* event) {
        return event->basic_line == 225 && event->address == 54277 && event->value == 29;
    });
    test_support::check(sweep != sid.end() && (*(sweep + 2))->basic_line == 226 &&
          (*(sweep + 2))->address == 54273 && (*(sweep + 2))->value == 55,
          "tornado sweep start/order");
    test_support::check((*(sweep + 18))->address == 54273 && (*(sweep + 18))->value == 151 &&
          (*(sweep + 20))->address == 54277 && (*(sweep + 20))->value == 0,
          "tornado sweep end/order");
}

void tornado_uses_shared_impact_and_bounds_paths() {
    WeatherAttackInput impact;
    impact.weapon = WeatherWeapon::tornado;
    impact.screen.frame.screen[1189 - 1024] = 104;
    const auto hit = weatherwar::simulate_weather_attack(impact);
    test_support::check(std::any_of(hit.events.begin(), hit.events.end(), [](const auto& event) {
        return event.kind == AttackEventKind::screen_write && event.basic_line == 111 &&
               event.value == 170;
    }), "tornado did not enter shared impact flash");
    test_support::check(std::any_of(hit.events.begin(), hit.events.end(), [](const auto& event) {
        return event.kind == AttackEventKind::sid_write && event.basic_line == 225;
    }), "tornado impact omitted post-flight sweep");

    WeatherAttackInput bounds;
    bounds.weapon = WeatherWeapon::tornado;
    bounds.screen.frame.screen[1189 - 1024] = 96;
    const auto outside = weatherwar::simulate_weather_attack(bounds);
    test_support::check(outside.nn == 1 && outside.ww == 0,
          "tornado out-of-bounds NN/WW state");
    test_support::check(std::none_of(outside.events.begin(), outside.events.end(), [](const auto& event) {
        return event.kind == AttackEventKind::sid_write && event.basic_line == 225;
    }), "out-of-bounds tornado incorrectly ran widening sweep");
    const auto text = events_of(outside.events, AttackEventKind::text_screen);
    test_support::check(text.size() == 3 && text[0]->basic_line == 85 &&
          text[1]->basic_line == 138 && text[2]->basic_line == 139,
          "tornado out-of-bounds text sequence");
}

} // namespace

int main() {
    trajectory_and_cleanup();
    rain_erases_only_drawn_cells();
    impact_audio_is_interleaved();
    out_of_bounds_text_and_audio();
    rejects_bad_bounds();
    rejects_nonfinite_numeric_inputs();
    lightning_zero_charge_vic_sid_and_text();
    lightning_direction_and_basic_arithmetic();
    tornado_shrinks_widens_and_sweeps();
    tornado_uses_shared_impact_and_bounds_paths();
    std::cout << "All weather trajectories, cleanup, text, VIC and SID tests passed.\n";
}
