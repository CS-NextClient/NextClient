#include "ncl_image/Encode.h"

#include <limits>
#include <utility>

#include <zlib.h>

namespace ncl_image
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
    } // namespace

    ncl_utils::ResultT<std::vector<uint8_t>> EncodePng(int width, int height, int channels, std::span<const uint8_t> pixels)
    {
        if (width <= 0 || height <= 0 || (channels != 1 && channels != 4))
        {
            return ncl_utils::ResultError("invalid PNG dimensions or channel count");
        }

        const uint64_t pixel_bytes = static_cast<uint64_t>(width) * height * channels;
        if (pixel_bytes != pixels.size() || pixel_bytes > std::numeric_limits<int>::max() ||
            pixel_bytes + height > std::numeric_limits<uLong>::max())
        {
            return ncl_utils::ResultError("PNG dimensions do not match the supplied pixel extent");
        }

        // Every row carries a filter byte. Filter 0 (none) throughout: these images are
        // written once and read by an editor, and choosing a filter per row would buy size
        // at the cost of the only part of this that could be got wrong.
        std::vector<uint8_t> raw;
        raw.reserve(static_cast<size_t>(height) * (1 + static_cast<size_t>(width) * channels));

        for (int y = 0; y < height; y++)
        {
            raw.push_back(0);

            const uint8_t* row = pixels.data() + static_cast<size_t>(y) * width * channels;
            raw.insert(raw.end(), row, row + static_cast<size_t>(width) * channels);
        }

        uLongf compressed_size = compressBound(static_cast<uLong>(raw.size()));
        std::vector<uint8_t> compressed(compressed_size);

        if (compress2(compressed.data(), &compressed_size, raw.data(), static_cast<uLong>(raw.size()), 9) != Z_OK)
        {
            return ncl_utils::ResultError("cannot compress the image");
        }

        compressed.resize(compressed_size);

        std::vector<uint8_t> file = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};

        std::vector<uint8_t> header;
        PushBigEndian(header, static_cast<uint32_t>(width));
        PushBigEndian(header, static_cast<uint32_t>(height));
        header.push_back(8); // bit depth
        header.push_back(channels == 1 ? 0 : 6); // gray, or RGBA
        header.push_back(0); // deflate
        header.push_back(0); // adaptive filtering
        header.push_back(0); // no interlace

        PushChunk(file, "IHDR", header);
        PushChunk(file, "IDAT", compressed);
        PushChunk(file, "IEND", {});

        return std::move(file);
    }
} // namespace ncl_image
