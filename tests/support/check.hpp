#pragma once

#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace test_support {

inline void check(bool condition, std::string_view message = "check failed",
                  const std::source_location where = std::source_location::current()) {
    if (!condition) {
        throw std::runtime_error(std::string(where.file_name()) + ":" +
                                 std::to_string(where.line()) + ": " + std::string(message));
    }
}

} // namespace test_support
