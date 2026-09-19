#include "basic/basic_random.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace weatherwar {
namespace {

constexpr std::uint64_t kMantissaTop = UINT64_C(1) << 31;
constexpr std::uint64_t kFacTop = UINT64_C(1) << 39;

// The two five-byte constants immediately before the RND routine at $E097.
constexpr BasicRandom::Seed kMultiplier{0x98, 0x35, 0x44, 0x7A, 0x00};
constexpr BasicRandom::Seed kIncrement{0x68, 0x28, 0xB1, 0x46, 0x00};

std::uint32_t memory_mantissa(const BasicRandom::Seed& value) {
    return UINT32_C(0x80000000) |
           (static_cast<std::uint32_t>(value[1] & 0x7F) << 24) |
           (static_cast<std::uint32_t>(value[2]) << 16) |
           (static_cast<std::uint32_t>(value[3]) << 8) |
           value[4];
}

BasicRandom::Seed store_fac(int exponent, std::uint64_t mantissa40) {
    if (mantissa40 == 0) {
        return {};
    }
    while (mantissa40 < kFacTop) {
        mantissa40 <<= 1;
        --exponent;
    }
    while (mantissa40 >= (kFacTop << 1)) {
        mantissa40 >>= 1;
        ++exponent;
    }

    std::uint64_t mantissa = mantissa40 >> 8;
    // $BBD4 calls $BC1B: bit 7 of the guard byte rounds away from zero.
    if ((mantissa40 & 0x80) != 0) {
        ++mantissa;
        if (mantissa == (UINT64_C(1) << 32)) {
            mantissa >>= 1;
            ++exponent;
        }
    }
    if (exponent <= 0) {
        return {};
    }
    if (exponent > 0xFF) {
        throw std::overflow_error("BASIC floating-point exponent overflow");
    }
    return {
        static_cast<std::uint8_t>(exponent),
        static_cast<std::uint8_t>((mantissa >> 24) & 0x7F),
        static_cast<std::uint8_t>(mantissa >> 16),
        static_cast<std::uint8_t>(mantissa >> 8),
        static_cast<std::uint8_t>(mantissa),
    };
}

double to_double(const BasicRandom::Seed& value) {
    if (value[0] == 0) {
        return 0.0;
    }
    const double magnitude = std::ldexp(
        static_cast<double>(memory_mantissa(value)),
        static_cast<int>(value[0]) - 160);
    return (value[1] & 0x80) != 0 ? -magnitude : magnitude;
}

BasicRandom::Seed multiply_positive(const BasicRandom::Seed& lhs,
                                    const BasicRandom::Seed& rhs) {
    if (lhs[0] == 0 || rhs[0] == 0) {
        return {};
    }
    const std::uint64_t product =
        static_cast<std::uint64_t>(memory_mantissa(lhs)) * memory_mantissa(rhs);
    int exponent = static_cast<int>(lhs[0]) + rhs[0] - 129;
    std::uint64_t mantissa40;
    if ((product & (UINT64_C(1) << 63)) != 0) {
        mantissa40 = product >> 24;
        ++exponent;
    } else {
        mantissa40 = product >> 23;
    }
    return store_fac(exponent, mantissa40);
}

BasicRandom::Seed integer_as_basic(std::uint8_t value) {
    if (value == 0) {
        return {};
    }
    const int highest_bit = 31 - std::countl_zero(static_cast<std::uint32_t>(value));
    const std::uint32_t mantissa = static_cast<std::uint32_t>(value) << (31 - highest_bit);
    return {
        static_cast<std::uint8_t>(highest_bit + 129),
        static_cast<std::uint8_t>((mantissa >> 24) & 0x7F),
        static_cast<std::uint8_t>(mantissa >> 16),
        static_cast<std::uint8_t>(mantissa >> 8),
        static_cast<std::uint8_t>(mantissa),
    };
}

BasicRandom::Seed advance(const BasicRandom::Seed& state) {
    // $BA28 keeps a 32-bit mantissa and an eight-bit guard.  Computing the
    // upper 40 product bits is the exact native equivalent for positive state.
    std::uint64_t mantissa40 = 0;
    int exponent = kIncrement[0];
    if (state[0] != 0) {
        const std::uint64_t product =
            static_cast<std::uint64_t>(memory_mantissa(state)) *
            memory_mantissa(kMultiplier);
        exponent = static_cast<int>(state[0]) + kMultiplier[0] - 129;
        if ((product & (UINT64_C(1) << 63)) != 0) {
            mantissa40 = product >> 24;
            ++exponent;
        } else {
            mantissa40 = product >> 23;
        }
    }

    // $B867 aligns and adds the positive increment, retaining the FAC's 40
    // bits. Bits shifted beyond the guard cannot carry back into the FAC.
    const std::uint64_t increment40 =
        static_cast<std::uint64_t>(memory_mantissa(kIncrement)) << 8;
    if (mantissa40 == 0) {
        mantissa40 = increment40;
        exponent = kIncrement[0];
    } else if (exponent >= kIncrement[0]) {
        const int shift = exponent - kIncrement[0];
        if (shift < 40) {
            mantissa40 += increment40 >> shift;
        }
    } else {
        const int shift = kIncrement[0] - exponent;
        mantissa40 = (shift < 40 ? mantissa40 >> shift : 0) + increment40;
        exponent = kIncrement[0];
    }
    if (mantissa40 >= (kFacTop << 1)) {
        mantissa40 >>= 1;
        ++exponent;
    }

    const std::uint32_t product_mantissa =
        static_cast<std::uint32_t>(mantissa40 >> 8);
    const std::uint32_t shuffled =
        ((product_mantissa & UINT32_C(0x000000FF)) << 24) |
        ((product_mantissa & UINT32_C(0x0000FF00)) << 8) |
        ((product_mantissa & UINT32_C(0x00FF0000)) >> 8) |
        ((product_mantissa & UINT32_C(0xFF000000)) >> 24);

    // $E0E3 replaces the FAC exponent with $80 but deliberately copies its
    // old exponent into the guard byte before $B8D7 normalizes and $BBD4 rounds.
    return store_fac(0x80,
                     (static_cast<std::uint64_t>(shuffled) << 8) |
                         static_cast<std::uint8_t>(exponent));
}

} // namespace

BasicRandom::BasicRandom(Seed seed) : seed_(seed) {
    seed_from_bytes(seed);
}

void BasicRandom::seed_from_bytes(Seed seed) {
    if (seed[0] != 0 && (seed[1] & 0x80) != 0) {
        throw std::invalid_argument("BASIC RND seed must be nonnegative");
    }
    seed_ = seed;
}

BasicRandom::Seed BasicRandom::seed() const noexcept {
    return seed_;
}

double BasicRandom::next() {
    seed_ = advance(seed_);
    return to_double(seed_);
}

double BasicRandom::next_scaled(std::uint8_t factor) {
    next();
    return to_double(multiply_positive(seed_, integer_as_basic(factor)));
}

double BasicRandom::round_to_basic(double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("BASIC floating-point value must be finite");
    }
    if (value == 0.0) {
        return 0.0;
    }
    const bool negative = value < 0.0;
    int binary_exponent = 0;
    const double fraction = std::frexp(std::abs(value), &binary_exponent);
    int exponent = binary_exponent + 128;
    std::uint64_t mantissa = static_cast<std::uint64_t>(
        std::floor(std::ldexp(fraction, 32) + 0.5));
    if (mantissa == (UINT64_C(1) << 32)) {
        mantissa >>= 1;
        ++exponent;
    }
    if (exponent <= 0) {
        return negative ? -0.0 : 0.0;
    }
    if (exponent > 0xFF) {
        throw std::overflow_error("BASIC floating-point exponent overflow");
    }
    Seed rounded{
        static_cast<std::uint8_t>(exponent),
        static_cast<std::uint8_t>(((mantissa >> 24) & 0x7F) |
                                  (negative ? 0x80 : 0x00)),
        static_cast<std::uint8_t>(mantissa >> 16),
        static_cast<std::uint8_t>(mantissa >> 8),
        static_cast<std::uint8_t>(mantissa),
    };
    return to_double(rounded);
}

} // namespace weatherwar
