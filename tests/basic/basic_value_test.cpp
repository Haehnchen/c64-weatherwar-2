#include "basic/basic_value.hpp"
#include "support/check.hpp"

#include <stdexcept>

namespace {

void test_required_rom_vectors() {
    test_support::check(weatherwar::basic_val("0") == 0.0, "VAL zero");
    test_support::check(weatherwar::basic_val("100") == 100.0, "VAL positive integer");
    test_support::check(weatherwar::basic_val("-150") == -150.0, "VAL negative integer");
    test_support::check(weatherwar::basic_val("150") == 150.0, "VAL integer");
    test_support::check(weatherwar::basic_val(".5") == 0.5, "VAL fraction");
    test_support::check(weatherwar::basic_val("1E2") == 100.0, "VAL exponent");
    test_support::check(weatherwar::basic_val("x") == 0.0, "VAL invalid input");
    test_support::check(weatherwar::basic_val("12foo") == 12.0, "VAL trailing text");
}

void test_chrget_space_and_termination_rules() {
    test_support::check(weatherwar::basic_val("  12") == 12.0, "VAL leading spaces");
    test_support::check(weatherwar::basic_val("1 2") == 12.0, "VAL embedded spaces");
    test_support::check(weatherwar::basic_val("1 2.3 4") == 12.339999999850988,
                        "VAL embedded spaces and fraction");
    test_support::check(weatherwar::basic_val("1 E + 2") == 100.0,
                        "VAL spaced exponent");
    test_support::check(weatherwar::basic_val("- . 5") == -0.5,
                        "VAL spaced negative fraction");
    test_support::check(weatherwar::basic_val("1\t2") == 1.0, "VAL tab termination");
    test_support::check(weatherwar::basic_val("+ x") == 0.0, "VAL invalid sign");
    test_support::check(weatherwar::basic_val(".") == 0.0, "VAL bare decimal point");
}

void test_exponent_and_basic_precision() {
    test_support::check(weatherwar::basic_val("1E") == 1.0, "VAL incomplete exponent");
    test_support::check(weatherwar::basic_val("1E+") == 1.0,
                        "VAL incomplete signed exponent");
    test_support::check(weatherwar::basic_val("1e2") == 1.0, "VAL lowercase exponent");
    test_support::check(weatherwar::basic_val("+12.34") == 12.339999999850988,
                        "VAL BASIC precision");
    test_support::check(weatherwar::basic_val("1E-50") == 0.0, "VAL underflow");
}

void test_overflow() {
    bool threw = false;
    try {
        (void)weatherwar::basic_val("1E39");
    } catch (const std::overflow_error&) {
        threw = true;
    }
    test_support::check(threw, "VAL overflow");
}

} // namespace

int main() {
    test_required_rom_vectors();
    test_chrget_space_and_termination_rules();
    test_exponent_and_basic_precision();
    test_overflow();
}
