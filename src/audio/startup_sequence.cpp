#include "audio/startup_sequence.hpp"
#include "audio/sid_chip.hpp"
#include "audio/sequence_data.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace weatherwar::audio {
namespace {
std::uint64_t read_le(std::span<const std::uint8_t> data, std::size_t& offset, unsigned count) {
    if (count > data.size() - std::min(offset, data.size()))
        throw std::runtime_error("Truncated SID sequence");
    std::uint64_t value = 0;
    for (unsigned i = 0; i < count; ++i) {
        value |= static_cast<std::uint64_t>(data[offset++]) << (8 * i);
    }
    return value;
}

StartupSequence parse_sequence(std::span<const std::uint8_t> data, std::string_view source) {
    std::size_t offset = 0;
    if (data.size() < 4 || data[0] != 'W' || data[1] != 'W' || data[2] != 'S' || data[3] != '1')
        throw std::runtime_error("Invalid SID sequence " + std::string(source));
    offset = 4;
    StartupSequence sequence{read_le(data, offset, 8), {}};
    const auto count = read_le(data, offset, 4);
    if (count == 0 || count > 100000 || sequence.end_cycle > 985248ULL * 120) {
        throw std::runtime_error("SID sequence exceeds opening limits");
    }
    sequence.events.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        SidEvent event{read_le(data, offset, 8), static_cast<std::uint8_t>(read_le(data, offset, 1)),
                       static_cast<std::uint8_t>(read_le(data, offset, 1))};
        if (event.reg > 24 || event.cycle > sequence.end_cycle ||
            (!sequence.events.empty() && event.cycle < sequence.events.back().cycle)) {
            throw std::runtime_error("Invalid ordered SID write");
        }
        sequence.events.push_back(event);
    }
    if (offset != data.size()) throw std::runtime_error("Trailing SID sequence bytes");
    return sequence;
}

void write_le(std::ostream& output, std::uint32_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i) output.put(static_cast<char>(value >> (8 * i)));
}
} // namespace

StartupSequence builtin_sequence(SequenceId id) {
    switch (id) {
    case SequenceId::startup: return parse_sequence(embedded::startup, "builtin startup");
    case SequenceId::entry: return parse_sequence(embedded::entry, "builtin entry");
    case SequenceId::first_name: return parse_sequence(embedded::first_name, "builtin first-name");
    case SequenceId::second_name: return parse_sequence(embedded::second_name, "builtin second-name");
    case SequenceId::board: return parse_sequence(embedded::board, "builtin board");
    case SequenceId::round: return parse_sequence(embedded::round, "builtin round");
    }
    throw std::runtime_error("Unknown built-in SID sequence");
}

StartupSequence load_startup_sequence(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open SID sequence " + path.string());
    std::vector<std::uint8_t> data;
    for (std::istreambuf_iterator<char> it(input), end; it != end; ++it) {
        data.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(*it)));
    }
    return parse_sequence(data, path.string());
}

std::vector<float> render_startup_sequence(const StartupSequence& sequence) {
    SidChip chip;
    return render_sequence(chip, sequence);
}

std::vector<float> render_sequence(SidChip& chip, const StartupSequence& sequence) {
    std::vector<float> pcm;
    std::uint64_t cursor = 0;
    auto advance = [&](std::uint64_t target) {
        if (target < cursor || target - cursor > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Invalid SID clock interval");
        }
        const auto chunk = chip.clock(static_cast<std::uint32_t>(target - cursor));
        pcm.insert(pcm.end(), chunk.begin(), chunk.end());
        cursor = target;
    };
    for (const auto& event : sequence.events) {
        advance(event.cycle);
        chip.write(event.reg, event.value);
    }
    advance(sequence.end_cycle);
    return pcm;
}

void write_wav(const std::filesystem::path& path, const std::vector<float>& pcm) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    const auto length = static_cast<std::uint32_t>(pcm.size() * 2);
    output.write("RIFF", 4); write_le(output, length + 36, 4);
    output.write("WAVEfmt ", 8); write_le(output, 16, 4);
    write_le(output, 1, 2); write_le(output, 1, 2); write_le(output, 48000, 4);
    write_le(output, 96000, 4); write_le(output, 2, 2); write_le(output, 16, 2);
    output.write("data", 4); write_le(output, length, 4);
    for (const auto sample : pcm) {
        const auto value = static_cast<std::int16_t>(std::lround(std::clamp(sample, -1.0F, 1.0F) * 32767));
        write_le(output, static_cast<std::uint16_t>(value), 2);
    }
    if (!output) throw std::runtime_error("Cannot write startup WAV");
}

void write_events(const std::filesystem::path& path, const StartupSequence& sequence) {
    std::ofstream output(path);
    for (const auto& event : sequence.events) {
        output << event.cycle << ' ' << static_cast<unsigned>(event.reg) << ' '
               << static_cast<unsigned>(event.value) << '\n';
    }
    if (!output) throw std::runtime_error("Cannot export SID events");
}

} // namespace weatherwar::audio
