#include "video/frame_export.hpp"

#include <fstream>
#include <stdexcept>

namespace weatherwar {

void export_frame(const CharacterFrame& frame, const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    auto save = [&](const char* name, const auto& values) {
        std::ofstream output(directory / name, std::ios::binary);
        output.write(reinterpret_cast<const char*>(values.data()), values.size());
        if (!output) throw std::runtime_error("Cannot export " + (directory / name).string());
    };
    save("screen.bin", frame.screen);
    save("colors.bin", frame.colors);
    save("charset.bin", frame.charset);
    save("vic-colors.bin", std::array<std::uint8_t, 2>{frame.border, frame.background});
    std::ofstream image(directory / "frame.ppm", std::ios::binary);
    image << "P6\n320 200\n255\n";
    for (const auto pixel : render_character_frame(frame)) {
        const char rgb[]{static_cast<char>(pixel >> 16), static_cast<char>(pixel >> 8),
                         static_cast<char>(pixel)};
        image.write(rgb, 3);
    }
    if (!image) throw std::runtime_error("Cannot export frame image");
}

} // namespace weatherwar
