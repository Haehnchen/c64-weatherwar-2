#pragma once

#include <array>
#include <cstdint>

namespace weatherwar {
std::array<std::uint8_t, 2048> character_rom(bool lowercase = false);
}
