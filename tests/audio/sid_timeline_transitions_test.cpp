#include "audio/sid_timeline.hpp"
#include "support/check.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

using weatherwar::audio::SidTimeline;
using weatherwar::audio::StartupSequence;

std::vector<float> render() {
    // The second tone deliberately omits volume. It is audible only when the
    // first scene's SID state survives the transition.
    const StartupSequence first{120000, {
        {0, 24, 15}, {0, 5, 9}, {0, 0, 80}, {0, 1, 20}, {0, 4, 17},
        {90000, 4, 16},
    }};
    const StartupSequence second{100000, {
        {0, 0, 140}, {0, 1, 30}, {0, 4, 17}, {70000, 4, 16},
    }};
    SidTimeline timeline;
    timeline.schedule(first, 0);
    timeline.schedule(second, 150000);
    constexpr std::uint64_t end = 300000;
    std::vector<float> pcm;
    for (const auto target : std::array<std::uint64_t, 6>{
             50000, 120000, 150000, 190000, 250000, end}) {
        auto part = timeline.render_until(target);
        pcm.insert(pcm.end(), part.begin(), part.end());
    }
    return pcm;
}

void test_continuous_timeline() {
    const auto continuous = render();
    test_support::check(!continuous.empty(), "continuous timeline produced no PCM");
    test_support::check(std::all_of(continuous.begin(), continuous.end(),
                      [](float value) { return std::isfinite(value); }),
          "continuous timeline produced non-finite PCM");
    const auto split = continuous.size() * 150000 / 300000;
    const auto peak = std::max_element(continuous.begin() + split, continuous.end(),
        [](float left, float right) { return std::abs(left) < std::abs(right); });
    test_support::check(peak != continuous.end() && std::abs(*peak) > 0.01F,
          "SID state was reset or second scene became silent");
    test_support::check(std::all_of(continuous.begin(), continuous.end(),
                      [](float value) { return std::abs(value) < 1.0F; }),
          "continuous timeline clipped");
}

int main() {
    test_continuous_timeline();
    std::cout << "Chunk-driven persistent SID transitions are finite, audible and unclipped.\n";
}
