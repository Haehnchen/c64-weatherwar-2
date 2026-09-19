#include "basic/basic_input.hpp"

namespace weatherwar {
namespace {

bool is_space(char c) {
    // CHRGET skips ASCII spaces only; TAB terminates VAL fields.
    return c == ' ';
}

std::string trim_trailing_spaces(std::string value) {
    while (!value.empty() && is_space(value.back())) {
        value.pop_back();
    }
    return value;
}

bool has_non_space(std::string_view text) {
    for (char c : text) {
        if (!is_space(c)) {
            return true;
        }
    }
    return false;
}

} // namespace

BasicInputField parse_basic_input_field(std::string_view raw_typed) {
    std::size_t pos = 0;
    while (pos < raw_typed.size() && is_space(raw_typed[pos])) {
        ++pos;
    }
    if (pos >= raw_typed.size()) {
        return {"", false};
    }
    if (raw_typed[pos] == '"') {
        ++pos;
        std::string value;
        bool closed = false;
        while (pos < raw_typed.size()) {
            char c = raw_typed[pos];
            if (c == '"') {
                closed = true;
                ++pos;
                break;
            }
            // Inside quotes commas, colons and spaces are literal.
            value.push_back(c);
            ++pos;
        }
        if (!closed) {
            // An unterminated quote consumes the rest of the line literally.
            return {value, false};
        }
        // After the closing quote BASIC ignores everything up to the next
        // comma; only data after that comma counts as extra input.
        while (pos < raw_typed.size() && raw_typed[pos] != ',') {
            ++pos;
        }
        if (pos >= raw_typed.size()) {
            return {value, false};
        }
        ++pos; // Skip the comma.
        return {value, has_non_space(raw_typed.substr(pos))};
    }
    std::string value;
    while (pos < raw_typed.size() && raw_typed[pos] != ',') {
        value.push_back(raw_typed[pos]);
        ++pos;
    }
    value = trim_trailing_spaces(value);
    if (pos >= raw_typed.size()) {
        return {value, false};
    }
    ++pos; // Skip the comma.
    return {value, has_non_space(raw_typed.substr(pos))};
}

} // namespace weatherwar
