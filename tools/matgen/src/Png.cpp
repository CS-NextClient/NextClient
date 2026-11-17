#include "Png.h"

#include <fstream>

#include <ncl_image/Encode.h>

namespace matgen
{
    Image MakeImage(int width, int height, int channels)
    {
        Image image;
        image.width = width;
        image.height = height;
        image.channels = channels;
        image.pixels.assign(static_cast<size_t>(width) * height * channels, 0);
        return image;
    }

    std::string WritePng(const std::string& path, const Image& image)
    {
        auto encoded = ncl_image::EncodePng(image.width, image.height, image.channels, image.pixels);
        if (encoded.has_error())
        {
            return encoded.error_str();
        }

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            return "cannot open " + path;
        }
        out.write(reinterpret_cast<const char*>(encoded->data()), static_cast<std::streamsize>(encoded->size()));
        return out ? std::string() : "cannot write " + path;
    }
} // namespace matgen
