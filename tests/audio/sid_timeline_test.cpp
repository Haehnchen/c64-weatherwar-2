#include "audio/sid_timeline.hpp"
#include "support/check.hpp"
#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>

template<class F> void rejects(F action) {
    bool threw = false;
    try { action(); } catch (const std::exception&) { threw = true; }
    test_support::check(threw, "expected SID timeline operation to throw");
}

void test_timeline_lifecycle() {
    using namespace weatherwar::audio;
    SidTimeline timeline;
    const StartupSequence tone{20000, {{0,24,15},{0,5,9},{1,0,100},{2,1,50},{3,4,17},{15000,4,16}}};
    timeline.schedule(tone, 0);
    test_support::check(timeline.pending_events() == 6);
    rejects([&] { timeline.schedule(tone, 19999); });
    test_support::check(timeline.pending_events() == 6);
    timeline.render_until(0);
    test_support::check(timeline.applied_events() == 2 && timeline.cycle() == 0);
    const auto first = timeline.render_until(10000);
    test_support::check(timeline.applied_events() == 5 && timeline.pending_events() == 1);
    const auto second = timeline.render_until(20000);
    test_support::check(timeline.applied_events() == 6 && timeline.pending_events() == 0);
    test_support::check(!first.empty() && !second.empty());
    // An idle input interval must advance the very same oscillator/filter state.
    const auto idle = timeline.render_until(985248);
    test_support::check(timeline.cycle() == 985248 && !idle.empty());
    const auto samples = first.size() + second.size() + idle.size();
    // The pinned two-pass fixed-point resampler yields approximately 48000,
    // not exactly 48000 samples (48011 in the local build).
    test_support::check(samples >= 47900 && samples <= 48100);
    test_support::check(std::all_of(idle.begin(), idle.end(), [](float v) { return std::isfinite(v); }));
    rejects([&] { timeline.render_until(985247); });
    rejects([&] { timeline.schedule(tone, 20000); });
    const auto before = timeline.pending_events();
    rejects([&] { timeline.schedule({10, {{0,4,17},{11,4,0}}}, 985248); });
    test_support::check(timeline.pending_events() == before);
    // Adjacent schedules keep the zero-distance event ordering at the boundary.
    timeline.schedule(tone, 985248);
    timeline.schedule(tone, 1005248);
    timeline.render_until(1005248);
    test_support::check(timeline.applied_events() == 14); // 6 initial +6 completed +2 at next start
    timeline.render_until(1025248);
    test_support::check(timeline.applied_events() == 18 && timeline.pending_events() == 0);
}

int main() {
    test_timeline_lifecycle();
    std::cout << "Persistent SID cycle clock, idle intervals and ordered adjacent schedules passed.\n";
}
