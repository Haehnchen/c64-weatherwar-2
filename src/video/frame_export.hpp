#pragma once

#include "video/character_frame.hpp"
#include <filesystem>

namespace weatherwar {
void export_frame(const CharacterFrame& frame, const std::filesystem::path& directory);
}
