#pragma once

#include <array>
#include <cstdint>

namespace weatherwar {

// Native implementation of the five-byte random state used by C64 BASIC V2.
class BasicRandom {
public:
    using Seed = std::array<std::uint8_t, 5>;

    [[nodiscard]] static constexpr Seed default_seed() noexcept {
        return {0x80, 0x4F, 0xC7, 0x52, 0x58};
    }

    explicit BasicRandom(Seed seed = default_seed());

    void seed_from_bytes(Seed seed);
    [[nodiscard]] Seed seed() const noexcept;

    // Advance exactly once as RND(1), returning the BASIC floating value.
    double next();

    // Advance exactly once and multiply in BASIC's five-byte format.
    double next_scaled(std::uint8_t factor);

    // Round a finite host value to BASIC's five-byte floating precision.
    [[nodiscard]] static double round_to_basic(double value);

private:
    Seed seed_;
};

} // namespace weatherwar
