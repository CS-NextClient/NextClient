#include "Png.h"

#include <cstring>
#include <fstream>

#include <zlib.h>

namespace matgen
{
    namespace
    {
        void PushBigEndian(std::vector<uint8_t>& out, uint32_t value)
        {
            out.push_back(static_cast<uint8_t>(value >> 24));
            out.push_back(static_cast<uint8_t>(value >> 16));
            out.push_back(static_cast<uint8_t>(value >> 8));
            out.push_back(static_cast<uint8_t>(value));
        }

        void PushChunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& body)
        {
            PushBigEndian(out, static_cast<uint32_t>(body.size()));

            size_t start = out.size();
            out.insert(out.end(), type, type + 4);
            out.insert(out.end(), body.begin(), body.end());

            uLong crc = crc32(0, Z_NULL, 0);
            crc = crc32(crc, out.data() + start, static_cast<uInt>(out.size() - start));

            PushBigEndian(out, static_cast<uint32_t>(crc));
        }
    }

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
        if (image.width <= 0 || image.height <= 0 || (image.channels != 1 && image.channels != 4))
        {
            return "refusing to write an image of " + std::to_string(image.width) + "x" +
                   std::to_string(image.height) + " with " + std::to_string(image.channels) + " channels";
        }

        // Every row carries a filter byte. Filter 0 (none) throughout: these images are
        // written once and read by an editor, and choosing a filter per row would buy size
        // at the cost of the only part of this that could be got wrong.
        std::vector<uint8_t> raw;
        raw.reserve(static_cast<size_t>(image.height) * (1 + static_cast<size_t>(image.width) * image.channels));

        for (int y = 0; y < image.height; y++)
        {
            raw.push_back(0);

            const uint8_t* row = image.pixels.data() + static_cast<size_t>(y) * image.width * image.channels;
            raw.insert(raw.end(), row, row + static_cast<size_t>(image.width) * image.channels);
        }

        uLongf compressed_size = compressBound(static_cast<uLong>(raw.size()));
        std::vector<uint8_t> compressed(compressed_size);

        if (compress2(compressed.data(), &compressed_size, raw.data(), static_cast<uLong>(raw.size()), 9) != Z_OK)
        {
            return "cannot compress the image";
        }

        compressed.resize(compressed_size);

        std::vector<uint8_t> file = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};

        std::vector<uint8_t> header;
        PushBigEndian(header, static_cast<uint32_t>(image.width));
        PushBigEndian(header, static_cast<uint32_t>(image.height));
        header.push_back(8);                                    // bit depth
        header.push_back(image.channels == 1 ? 0 : 6);          // grey, or RGBA
        header.push_back(0);                                    // deflate
        header.push_back(0);                                    // adaptive filtering
        header.push_back(0);                                    // no interlace

        PushChunk(file, "IHDR", header);
        PushChunk(file, "IDAT", compressed);
        PushChunk(file, "IEND", {});

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            return "cannot open " + path;
        }

        out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));

        return out ? std::string() : "cannot write " + path;
    }
}
