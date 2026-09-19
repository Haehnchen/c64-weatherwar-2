#include "assets/character_rom.hpp"
#include "assets/character_data.hpp"

#include <cstddef>

namespace weatherwar {

std::array<std::uint8_t, 2048> character_rom(bool lowercase) {
    std::array<std::uint8_t, 2048> result{};
    const auto offset = lowercase ? std::size_t{2048} : std::size_t{0};
    for (std::size_t index = 0; index < result.size(); ++index)
        result[index] = assets::character_data[offset + index];
    return result;
}

} // namespace weatherwar
