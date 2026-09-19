#pragma once

#include <string_view>

namespace weatherwar {

// Parse a string with Commodore BASIC V2 VAL semantics and return the value
// rounded to BASIC's five-byte floating-point precision.
double basic_val(std::string_view text);

} // namespace weatherwar
