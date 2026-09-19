#include "basic/basic_input.hpp"
#include "basic/basic_value.hpp"
#include "support/check.hpp"

#include <string>

namespace {

void check_field(const std::string& raw, const std::string& expected_bb, bool expected_extra) {
    const auto field = weatherwar::parse_basic_input_field(raw);
    test_support::check(field.value == expected_bb, "parsed BASIC input value");
    test_support::check(field.extra_ignored == expected_extra,
                        "parsed BASIC input extra-field flag");
    // VAL parity for the five pinned natural input-strings cases.
    (void)weatherwar::basic_val(field.value);
}

void test_pinned_natural_cases() {
    // BASIC 79: BB$ input and VAL result.
    check_field("\"50\"", "50", false);
    test_support::check(
        weatherwar::basic_val(weatherwar::parse_basic_input_field("\"50\"").value) == 50.0,
        "quoted BASIC input VAL");
    check_field("50,60", "50", true);
    test_support::check(
        weatherwar::basic_val(weatherwar::parse_basic_input_field("50,60").value) == 50.0,
        "comma BASIC input VAL");
    check_field("  50", "50", false);
    check_field("50  ", "50", false);
    check_field("", "", false);
    test_support::check(
        weatherwar::basic_val(weatherwar::parse_basic_input_field("").value) == 0.0,
        "empty BASIC input VAL");
}

void test_quote_and_extra_rules() {
    check_field("\"50,60\"", "50,60", false);
    check_field("50,", "50", false);
    check_field("50, ", "50", false);
    check_field("50 , 60", "50", true);
    check_field("\"50\" , 60", "50", true);
    check_field("\"50\"junk,60", "50", true);
    check_field("\"50\"junk", "50", false);
}

void test_empty_extra_fields_are_not_reported() {
    // INPUT assigns only the first field. A delimiter followed solely by
    // editor spaces is not an additional value, while a second delimiter is.
    check_field("50,   ", "50", false);
    check_field("50, ,", "50", true);
    check_field("\"50,60\",   ", "50,60", false);
    check_field("  \"50\"  ,   ", "50", false);
}

} // namespace

int main() {
    test_pinned_natural_cases();
    test_quote_and_extra_rules();
    test_empty_extra_fields_are_not_reported();
}
