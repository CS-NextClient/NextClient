#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace matgen
{
    struct Image
    {
        int width{};
        int height{};
        int channels{}; // 1 for gray, 4 for RGBA
        std::vector<uint8_t> pixels;

        uint8_t* At(int x, int y)
        {
            return &pixels[(static_cast<size_t>(y) * width + x) * channels];
        }
    };

    Image MakeImage(int width, int height, int channels);

    // Returns an error message, empty on success.
    std::string WritePng(const std::string& path, const Image& image);
} // namespace matgen
