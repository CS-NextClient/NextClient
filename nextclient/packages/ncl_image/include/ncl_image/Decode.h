#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include <ncl_utils/safe_result.h>

namespace ncl_image
{
    struct RgbaPixelsDeleter
    {
        void operator()(uint8_t* pixels) const noexcept;
    };

    struct RgbaImage
    {
        int width{};
        int height{};
        std::unique_ptr<uint8_t, RgbaPixelsDeleter> pixels;
    };

    enum class DecodeErrorCode
    {
        InputSize,
        HeaderDimensions,
        Decoder,
        DecodedDimensions
    };

    class DecodeError : public ncl_utils::ResultError
    {
    public:
        DecodeErrorCode code;
        int width{};
        int height{};

        DecodeError(DecodeErrorCode code, std::string reason, int width = 0, int height = 0);
    };

    ncl_utils::ResultT<RgbaImage, DecodeError> DecodeRgba8(std::span<const uint8_t> encoded, uint64_t max_pixels);
}
