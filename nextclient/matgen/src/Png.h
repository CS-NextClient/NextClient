#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace matgen
{
    // Minimal PNG writer: what a tool needs to hand an image to an image editor and to the
    // client's own loader, and nothing else. The client reads PNG through stb, which is a
    // decoder only, so the writing side is here rather than in a package: nothing else in
    // the tree writes images.
    struct Image
    {
        int width = 0;
        int height = 0;
        int channels = 0; // 1 for grey, 4 for RGBA
        std::vector<uint8_t> pixels;

        uint8_t* At(int x, int y) { return &pixels[(static_cast<size_t>(y) * width + x) * channels]; }
    };

    Image MakeImage(int width, int height, int channels);

    // Returns an error message, empty on success.
    std::string WritePng(const std::string& path, const Image& image);
}
