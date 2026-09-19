#include "basic/basic_value.hpp"

#include "basic/basic_random.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace weatherwar {
namespace {

class BasicValParser {
public:
    explicit BasicValParser(std::string_view text) : text_(text) {}

    double parse() {
        char current = next_non_space();
        bool negative = false;
        if (current == '-' || current == '+') {
            negative = current == '-';
            current = next_non_space();
        }

        double value = 0.0;
        int fractional_digits = 0;
        bool after_decimal = false;
        bool saw_decimal = false;
        while (true) {
            if (current >= '0' && current <= '9') {
                value = BasicRandom::round_to_basic(value * 10.0);
                value = BasicRandom::round_to_basic(
                    value + static_cast<double>(current - '0'));
                if (after_decimal) {
                    if (fractional_digits < 1000) {
                        ++fractional_digits;
                    }
                }
                current = next_non_space();
                continue;
            }
            if (current == '.' && !saw_decimal) {
                saw_decimal = true;
                after_decimal = true;
                current = next_non_space();
                continue;
            }
            break;
        }

        int decimal_exponent = 0;
        if (current == 'E') {
            current = next_non_space();
            bool exponent_negative = false;
            if (current == '-' || current == '+') {
                exponent_negative = current == '-';
                current = next_non_space();
            }
            while (current >= '0' && current <= '9') {
                // The ROM rejects a third significant positive exponent digit
                // at $BD91 and clamps a long negative exponent to underflow.
                if (decimal_exponent >= 10) {
                    if (!exponent_negative) {
                        throw std::overflow_error("BASIC VAL exponent overflow");
                    }
                    decimal_exponent = 100;
                    while (current >= '0' && current <= '9') {
                        current = next_non_space();
                    }
                    break;
                }
                decimal_exponent = decimal_exponent * 10 + (current - '0');
                current = next_non_space();
            }
            if (exponent_negative) {
                decimal_exponent = -decimal_exponent;
            }
        }

        int scale = decimal_exponent - fractional_digits;
        while (scale > 0) {
            value = BasicRandom::round_to_basic(value * 10.0);
            --scale;
        }
        while (scale < 0 && value != 0.0) {
            value = BasicRandom::round_to_basic(value / 10.0);
            ++scale;
        }
        if (value == 0.0) {
            return 0.0;
        }
        return negative ? -value : value;
    }

private:
    char next_non_space() {
        while (position_ < text_.size() && text_[position_] == ' ') {
            ++position_;
        }
        if (position_ == text_.size()) {
            return '\0';
        }
        return text_[position_++];
    }

    std::string_view text_;
    std::size_t position_ = 0;
};

} // namespace

double basic_val(std::string_view text) {
    return BasicValParser(text).parse();
}

} // namespace weatherwar
