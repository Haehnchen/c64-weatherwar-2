#pragma once

#include <string>
#include <string_view>

namespace weatherwar {

// BASIC line 79 INPUT assignment for BB$. The first field is trimmed or
// unquoted; later comma-separated data produces ROM message $ACFC.
struct BasicInputField {
    std::string value;
    bool extra_ignored = false;
};

BasicInputField parse_basic_input_field(std::string_view raw_typed);

} // namespace weatherwar
