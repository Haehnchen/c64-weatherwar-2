#include "audio/startup_sequence.hpp"
#include "support/check.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using weatherwar::audio::SidEvent;
using weatherwar::audio::StartupSequence;

void append_le(std::vector<std::uint8_t>& data, std::uint64_t value, unsigned bytes) {
    for (unsigned index = 0; index < bytes; ++index) {
        data.push_back(static_cast<std::uint8_t>(value >> (index * 8)));
    }
}

std::vector<std::uint8_t> encode(StartupSequence sequence, bool trailing = false) {
    std::vector<std::uint8_t> data{'W', 'W', 'S', '1'};
    append_le(data, sequence.end_cycle, 8);
    append_le(data, sequence.events.size(), 4);
    for (const auto event : sequence.events) {
        append_le(data, event.cycle, 8);
        append_le(data, event.reg, 1);
        append_le(data, event.value, 1);
    }
    if (trailing) data.push_back(0);
    return data;
}

void write_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& data) {
    std::ofstream output(path, std::ios::binary);
    test_support::check(output.good(), "cannot create temporary SID sequence");
    output.write(reinterpret_cast<const char*>(data.data()),
                 static_cast<std::streamsize>(data.size()));
    test_support::check(output.good(), "cannot write temporary SID sequence");
}

std::filesystem::path unique_temp_directory() {
    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    for (std::uint64_t attempt = 0; attempt < 1000; ++attempt) {
        const auto name = "weatherwar-startup-sequence-" + std::to_string(stamp + attempt);
        const auto directory = std::filesystem::temp_directory_path() / name;
        std::error_code error;
        if (std::filesystem::create_directory(directory, error)) return directory;
    }
    throw std::runtime_error("cannot create a unique temporary directory");
}

void expect_rejected(const std::filesystem::path& path, const char* case_name) {
    try {
        static_cast<void>(weatherwar::audio::load_startup_sequence(path));
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error(std::string(case_name) + " was accepted");
}

void test_builtin_startup() {
    const auto sequence = weatherwar::audio::builtin_sequence(weatherwar::audio::SequenceId::startup);
    test_support::check(sequence.end_cycle > 0 && !sequence.events.empty(),
                        "startup sequence has no bounded duration or events");
    test_support::check(sequence.events.front().cycle == 0 && sequence.events.front().reg == 24 &&
                sequence.events.front().value == 15,
                        "startup sequence does not begin with the original volume write");
    test_support::check(std::is_sorted(sequence.events.begin(), sequence.events.end(),
                           [](const SidEvent& left, const SidEvent& right) {
                               return left.cycle < right.cycle;
                           }),
                        "startup events are not sorted");
    const auto pcm = weatherwar::audio::render_startup_sequence(sequence);
    test_support::check(!pcm.empty(), "startup sequence rendered no samples");
    const auto nonzero = std::any_of(pcm.begin(), pcm.end(), [](float sample) {
        return std::isfinite(sample) && std::abs(sample) > 0.001F;
    });
    test_support::check(nonzero, "startup sequence rendered silence");
    float peak = 0.0F;
    for (const auto sample : pcm) {
        test_support::check(std::isfinite(sample), "startup PCM contains a non-finite sample");
        peak = std::max(peak, std::abs(sample));
    }
    test_support::check(peak <= 1.5F, "startup PCM exceeds a sensible normalized amplitude");
    const auto expected_samples = static_cast<std::int64_t>(
        std::llround(static_cast<double>(sequence.end_cycle) * 48000.0 / 985248.0));
    const auto sample_difference = std::llabs(static_cast<std::int64_t>(pcm.size()) - expected_samples);
    test_support::check(sample_difference <= 256, "startup PCM duration is outside resampler latency tolerance");
}

void test_builtin_metadata() {
    using weatherwar::audio::SequenceId;
    struct Expected { SequenceId id; std::uint64_t end_cycle; std::size_t events; };
    constexpr std::array expected{
        Expected{SequenceId::startup, 5817797, 366},
        Expected{SequenceId::entry, 892694, 10},
        Expected{SequenceId::first_name, 892722, 10},
        Expected{SequenceId::second_name, 892621, 10},
        Expected{SequenceId::board, 932540, 10},
        Expected{SequenceId::round, 932800, 10},
    };
    for (const auto item : expected) {
        const auto sequence = weatherwar::audio::builtin_sequence(item.id);
        test_support::check(sequence.end_cycle == item.end_cycle && sequence.events.size() == item.events,
                            "built-in SID metadata differs from the pinned assets");
    }
}

void test_rejections(const std::filesystem::path& directory) {
    const StartupSequence valid{100, {{0, 0, 1}, {10, 1, 2}}};
    const auto valid_data = encode(valid);

    auto corrupt_signature = valid_data;
    corrupt_signature[0] = 'X';
    write_bytes(directory / "corrupt-signature.sid", corrupt_signature);
    expect_rejected(directory / "corrupt-signature.sid", "corrupt signature");

    auto truncated_header = valid_data;
    truncated_header.resize(3);
    write_bytes(directory / "truncated-header.sid", truncated_header);
    expect_rejected(directory / "truncated-header.sid", "truncated header");

    auto truncated_event = valid_data;
    truncated_event.pop_back();
    write_bytes(directory / "truncated-event.sid", truncated_event);
    expect_rejected(directory / "truncated-event.sid", "truncated event");

    const StartupSequence unsorted{100, {{10, 0, 1}, {9, 1, 2}}};
    write_bytes(directory / "unsorted.sid", encode(unsorted));
    expect_rejected(directory / "unsorted.sid", "unsorted events");

    const StartupSequence invalid_register{100, {{0, 25, 1}}};
    write_bytes(directory / "register-25.sid", encode(invalid_register));
    expect_rejected(directory / "register-25.sid", "register 25");

    write_bytes(directory / "trailing.sid", encode(valid, true));
    expect_rejected(directory / "trailing.sid", "trailing bytes");
}

} // namespace

int main(int argc, char**) {
    if (argc != 1) {
        std::cerr << "Usage: startup_sequence_test\n";
        return 2;
    }
    try {
        test_builtin_startup();
        test_builtin_metadata();
        const auto directory = unique_temp_directory();
        try {
            test_rejections(directory);
        } catch (...) {
            std::filesystem::remove_all(directory);
            throw;
        }
        std::filesystem::remove_all(directory);
    } catch (const std::exception& error) {
        std::cerr << "startup_sequence_test: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
