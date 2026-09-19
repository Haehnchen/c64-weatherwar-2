#include "audio/sid_timeline.hpp"
#include "audio/startup_sequence.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            std::cerr << "usage: audio_transition_check INPUT.sid OUTPUT.wav\n";
            return 2;
        }
        const auto sequence = weatherwar::audio::load_startup_sequence(argv[1]);
        weatherwar::audio::SidTimeline timeline;
        timeline.schedule(sequence, 0);
        std::vector<float> pcm;
        constexpr std::uint64_t chunk_cycles = 65537;
        while (timeline.cycle() < sequence.end_cycle) {
            const auto target = std::min(sequence.end_cycle,
                                         timeline.cycle() + chunk_cycles);
            auto part = timeline.render_until(target);
            pcm.insert(pcm.end(), part.begin(), part.end());
        }
        if (timeline.applied_events() != sequence.events.size())
            throw std::runtime_error("not all SID events were synthesized");
        weatherwar::audio::write_wav(argv[2], pcm);
        std::cout << sequence.events.size() << " SID events, " << pcm.size()
                  << " samples, one persistent chip\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
