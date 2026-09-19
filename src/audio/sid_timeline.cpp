#include "audio/sid_timeline.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace weatherwar::audio {
void SidTimeline::schedule(const StartupSequence& sequence, std::uint64_t start_cycle) {
    if (start_cycle < cycle_ || start_cycle < scheduled_end_)
        throw std::invalid_argument("SID sequence overlaps rendered or scheduled time");
    if (sequence.end_cycle > std::numeric_limits<std::uint64_t>::max() - start_cycle)
        throw std::overflow_error("SID timeline timestamp overflow");
    std::uint64_t previous = 0;
    for (const auto& event : sequence.events) {
        if (event.reg > 24 || event.cycle < previous || event.cycle > sequence.end_cycle)
            throw std::invalid_argument("Invalid SID timeline event");
        previous = event.cycle;
    }
    // Validate the entire sequence before mutating the queue.
    for (const auto& event : sequence.events)
        events_.push_back({start_cycle + event.cycle, event.reg, event.value});
    scheduled_end_ = start_cycle + sequence.end_cycle;
}

std::vector<float> SidTimeline::render_until(std::uint64_t end_cycle) {
    if (end_cycle < cycle_) throw std::invalid_argument("SID clock cannot move backwards");
    std::vector<float> pcm;
    auto advance = [&](std::uint64_t target) {
        while (cycle_ < target) {
            const auto count = static_cast<std::uint32_t>(std::min<std::uint64_t>(target - cycle_, 985248));
            auto chunk = chip_.clock(count);
            pcm.insert(pcm.end(), chunk.begin(), chunk.end());
            cycle_ += count;
        }
    };
    while (!events_.empty() && events_.front().cycle <= end_cycle) {
        const auto event = events_.front();
        advance(event.cycle);
        chip_.write(event.reg, event.value);
        events_.pop_front();
        ++applied_events_;
    }
    advance(end_cycle);
    return pcm;
}
}
