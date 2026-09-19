#pragma once
#include "audio/sid_chip.hpp"
#include "audio/startup_sequence.hpp"
#include <cstdint>
#include <deque>
#include <vector>

namespace weatherwar::audio {
// One persistent PAL SID clock, including silence/input waits. Events are
// absolute cycle timestamps; rendering never clocks a disposable future tail.
// Device presentation/latency is a platform concern, not inferred here.
class SidTimeline {
public:
    void schedule(const StartupSequence& sequence, std::uint64_t start_cycle);
    std::vector<float> render_until(std::uint64_t end_cycle);
    std::uint64_t cycle() const { return cycle_; }
    std::uint64_t scheduled_end() const { return scheduled_end_; }
    std::uint64_t applied_events() const { return applied_events_; }
    std::size_t pending_events() const { return events_.size(); }
private:
    SidChip chip_;
    std::deque<SidEvent> events_;
    std::uint64_t cycle_ = 0;
    std::uint64_t scheduled_end_ = 0;
    std::uint64_t applied_events_ = 0;
};
}
