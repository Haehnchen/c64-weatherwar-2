#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace weatherwar::audio {

// Cycle-driven MOS6581 configured for the PAL C64 clock and 48 kHz output.
// The wrapper owns the pinned reSIDfp state and exposes only the register and
// clock operations needed by the native port.
class SidChip {
public:
    SidChip();
    ~SidChip();

    SidChip(const SidChip&) = delete;
    SidChip& operator=(const SidChip&) = delete;
    SidChip(SidChip&&) noexcept;
    SidChip& operator=(SidChip&&) noexcept;

    void reset();
    void write(std::uint8_t reg, std::uint8_t value);

    // Advance by PAL CPU cycles and return newly resampled mono samples in
    // the normal float PCM range (-1.0 .. 1.0).
    [[nodiscard]] std::vector<float> clock(std::uint32_t cycles);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace weatherwar::audio
