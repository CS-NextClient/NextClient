#include "ncl_image/Decode.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

#include <stb/stb_image.h>

namespace ncl_image
{
    namespace
    {
        constexpr uint64_t kMaxRgbaPixels = std::min<uint64_t>(
            std::numeric_limits<size_t>::max() / 4,
            std::numeric_limits<int>::max() / 4
        );

        bool AreAdmissibleDimensions(int width, int height, uint64_t max_pixels)
        {
            return width > 0 && height > 0 && static_cast<uint64_t>(width) <= max_pixels / height &&
                   static_cast<uint64_t>(width) <= kMaxRgbaPixels / height;
        }
    }

    void RgbaPixelsDeleter::operator()(uint8_t* pixels) const noexcept
    {
        stbi_image_free(pixels);
    }

    DecodeError::DecodeError(DecodeErrorCode code, std::string reason, int width, int height) :
        ncl_utils::ResultError(std::move(reason)),
        code(code),
        width(width),
        height(height)
    {}

    ncl_utils::ResultT<RgbaImage, DecodeError> DecodeRgba8(std::span<const uint8_t> encoded, uint64_t max_pixels)
    {
        if (encoded.empty() || encoded.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        {
            return DecodeError(DecodeErrorCode::InputSize, "encoded image size must fit a positive signed integer");
        }

        int length = static_cast<int>(encoded.size());
        int width = 0;
        int height = 0;
        int components = 0;

        if (!stbi_info_from_memory(encoded.data(), length, &width, &height, &components) ||
            !AreAdmissibleDimensions(width, height, max_pixels))
        {
            return DecodeError(DecodeErrorCode::HeaderDimensions, "invalid image header or dimensions", width, height);
        }

        RgbaImage image;
        image.pixels.reset(stbi_load_from_memory(encoded.data(), length, &image.width, &image.height, &components, STBI_rgb_alpha));

        if (image.pixels == nullptr)
        {
            const char* reason = stbi_failure_reason();
            return DecodeError(DecodeErrorCode::Decoder, reason != nullptr ? reason : "unknown image decode error");
        }

        if (!AreAdmissibleDimensions(image.width, image.height, max_pixels))
        {
            return DecodeError(DecodeErrorCode::DecodedDimensions, "decoded image dimensions exceed the budget", image.width, image.height);
        }

        return std::move(image);
    }
}
