#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace weatherwar::audio {
class SidChip;

struct SidEvent {
    std::uint64_t cycle;
    std::uint8_t reg;
    std::uint8_t value;
};

struct StartupSequence {
    std::uint64_t end_cycle;
    std::vector<SidEvent> events;
};

enum class SequenceId { startup, entry, first_name, second_name, board, round };

StartupSequence builtin_sequence(SequenceId id);
StartupSequence load_startup_sequence(const std::filesystem::path& path);
std::vector<float> render_startup_sequence(const StartupSequence& sequence);
std::vector<float> render_sequence(SidChip& chip, const StartupSequence& sequence);
void write_wav(const std::filesystem::path& path, const std::vector<float>& pcm);
void write_events(const std::filesystem::path& path, const StartupSequence& sequence);

} // namespace weatherwar::audio
