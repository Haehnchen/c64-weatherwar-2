#include "weather/attack_timing.hpp"
#include "weather/attack_timing_data.hpp"
#include "weather/nature_timing_data.hpp"
#include <algorithm>
#include <string>

namespace weatherwar {
std::uint64_t calibrated_loop_cycles(unsigned count) {
    return static_cast<std::uint64_t>(count) * attack_timing_data::loop_cycles;
}
namespace {
bool calibrate_nature(std::span<const AttackEvent> events, AttackSchedule& result) {
    using namespace nature_timing_data;
    if (events.size() != 290) return false;
    for (std::size_t tone = 0; tone < 4; ++tone) {
        const auto base = tone * 72;
        const auto& snapshot = events[base];
        if (snapshot.kind != AttackEventKind::text_screen || !snapshot.text_screen
            || snapshot.basic_line != (tone % 2 ? 57 : 56)) return false;
        for (std::size_t j = 0; j < 70; ++j) {
            const auto& event = events[base + 1 + j];
            const auto& store = sid_stores[tone * 70 + j];
            if (event.kind != AttackEventKind::sid_write || event.basic_line != store.basic_line
                || event.address != store.address || event.value != store.value) return false;
        }
        const auto& delay = events[base + 71];
        if (delay.kind != AttackEventKind::delay || delay.basic_line != 136
            || delay.delay_loop_count != 250) return false;
    }
    if (events[288].kind != AttackEventKind::delay || events[288].basic_line != 135
        || events[288].delay_loop_count != 1000 || events[289].kind != AttackEventKind::text_screen
        || events[289].basic_line != 58 || !events[289].text_screen) return false;
    result.event_cycles.resize(events.size());
    result.end_cycle = sid_stores.back().cycle + checkpoint_intervals[4].cycles;
    for (std::size_t tone = 0; tone < 4; ++tone) {
        const auto base = tone * 72;
        // PRINT completion was not separately captured: present its snapshot at
        // the following first store, the latest proven completed boundary.
        result.event_cycles[base] = sid_stores[tone * 70].cycle;
        for (std::size_t j = 0; j < 70; ++j)
            result.event_cycles[base + 1 + j] = sid_stores[tone * 70 + j].cycle;
        const auto end = sid_stores[tone * 70 + 69].cycle;
        const auto next = tone < 3 ? sid_stores[(tone + 1) * 70].cycle : result.end_cycle;
        result.event_cycles[base + 71] = tone < 3 ? next : end + (next - end) / 5;
    }
    result.event_cycles[288] = result.end_cycle;
    result.event_cycles[289] = result.end_cycle;
    result.interpolated_boundaries = 10;
    result.nature_sid_calibrated = true;
    return true;
}
std::string signature(const AttackEvent& event) {
    return std::to_string(event.basic_line) + "/" + std::to_string(static_cast<unsigned>(event.kind)) + "/"
        + std::to_string(event.kind == AttackEventKind::sid_write || event.kind == AttackEventKind::vic_write ? event.address : 0);
}
}
AttackSchedule schedule_attack(const WeatherAttackResult& attack) {
    return schedule_events(attack.events);
}
AttackSchedule schedule_events(std::span<const AttackEvent> events) {
    using namespace attack_timing_data;
    AttackSchedule result;
    if (calibrate_nature(events, result)) return result;
    result.event_cycles.resize(events.size());
    std::string previous = "START";
    std::vector<std::size_t> pending;
    std::uint64_t cursor = 0;
    for (std::size_t i = 0; i <= events.size(); ++i) {
        const bool end = i == events.size();
        if (!end && events[i].kind != AttackEventKind::sid_write && events[i].kind != AttackEventKind::screen_write && events[i].kind != AttackEventKind::vic_write) {
            pending.push_back(i); continue;
        }
        const auto next = end ? std::string("END") : signature(events[i]);
        std::string key = previous + "|";
        std::uint64_t weight = 0;
        for (std::size_t j = 0; j < pending.size(); ++j) {
            const auto& e = events[pending[j]];
            if (j) key += ',';
            key += std::to_string(e.basic_line) + "/" + std::to_string(static_cast<unsigned>(e.kind)) + "/" + std::to_string(e.delay_loop_count);
            weight += e.kind == AttackEventKind::delay ? loop_cycles * e.delay_loop_count : print_weight;
        }
        key += "|" + next;
        const auto exact = std::find_if(intervals.begin(), intervals.end(), [&](const auto& row) { return row.key == key; });
        const auto alternative = std::find_if(fallback.begin(), fallback.end(), [&](const auto& row) { return row.key == next; });
        // A missing path is counted and reported. The generic store fallback is
        // a measured corpus median, never a claim of exact execution timing.
        const auto store_weight = alternative != fallback.end() ? alternative->cycles : default_store_cycles;
        std::uint64_t delta = weight + (end ? 0 : store_weight);
        if (exact != intervals.end()) delta = exact->cycles;
        else ++result.fallback_intervals;
        std::uint64_t consumed_weight = 0;
        for (auto index : pending) {
            const auto& e = events[index];
            consumed_weight += e.kind == AttackEventKind::delay ? loop_cycles * e.delay_loop_count : print_weight;
            result.event_cycles[index] = cursor + (weight + store_weight ? delta * consumed_weight / (weight + store_weight) : 0);
        }
        cursor += delta;
        if (!end) result.event_cycles[i] = cursor;
        previous = next; pending.clear();
    }
    result.end_cycle = cursor;
    return result;
}
}
