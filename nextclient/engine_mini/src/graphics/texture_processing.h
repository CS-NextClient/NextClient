#pragma once
#include <cstdint>

#include "gl_local.h"

namespace tex
{
    constexpr int kTextureMaxSize = 1024;

    /// Calculates the optimal Power-Of-Two texture dimensions based on
    /// hardware support and cvar limits (gl_picmip, gl_max_size, gl_round_down).
    void GetAdjustedTexturePOTSize(int width, int height, int* width_out, int* height_out);

    /// Applies gamma correction to a color palette using the engine's texgammatable.
    void ApplyGammaToPalette(const uint8_t* data, uint8_t* data_out, int count);

    /// Converts 8-bit indexed texture data into 32-bit RGBA using the provided palette.
    /// Handles Opaque, Alpha, AlphaGradient and RGBA indexed formats.
    /// Optional dithering adds low-bit noise to reduce banding on Opaque textures.
    /// Returns the effective format after conversion (e.g. Alpha may become Opaque
    /// if all pixels are fully opaque), or TextureFormat::Invalid on failure.
    TextureFormat ConvertIndexedToRGBA(
        const uint8_t* indices,
        int count,
        const uint8_t* palette,
        TextureFormat format,
        bool dither,
        uint8_t* data_out
    );

    /// Resamples an indexed (1 bpp) texture to new dimensions using
    /// point sampling (nearest neighbor) with 16.16 fixed-point stepping.
    void ResamplePointIndexed(const uint8_t* data, int width, int height, uint8_t* data_out, int scaled_width, int scaled_height);

    /// Generates the next mipmap level for an RGBA texture by averaging 2x2 pixel blocks.
    /// data_out may alias data for in-place downsampling.
    /// Returns the height of the produced mip level.
    int MipMapRGBA(const uint8_t* data, int width, int height, uint8_t* data_out);

    /// Computes the average RGB of opaque neighbors in a 3x3 kernel around (x, y),
    /// writing the result with alpha=0 to data_out. Fully transparent pixels are skipped.
    /// Returns the blue channel of the result.
    int BoxFilter3x3RGBA(const uint8_t* data, int width, int height, int x, int y, uint8_t* data_out);

    /// Resamples an RGBA texture to new dimensions using bilinear interpolation
    /// (2x2 sample grid per output pixel).
    void ResampleRGBA(const uint8_t* data, int width, int height, uint8_t* data_out, int scaled_width, int scaled_height);

    /// Resamples a grayscale (1 bpp) texture to new dimensions using bilinear interpolation.
    void ResampleGrayscale(const uint8_t* data, int width, int height, uint8_t* data_out, int scaled_width, int scaled_height);

    /// Applies gamma correction to palette, then converts indexed texture data
    /// to 32-bit RGBA via ConvertIndexedToRGBA. Returns false if the palette is
    /// missing, the format is not indexed, or the conversion fails.
    bool TryUnpackTextureToRGBA(
        const uint8_t* data,
        TextureFormat format,
        int width,
        int height,
        const uint8_t* palette,
        uint8_t* data_out
    );

    /// Fills fully transparent (RGBA=0) pixels with the average RGB of their
    /// opaque 3x3 neighbors (via BoxFilter3x3RGBA), keeping alpha=0.
    /// Eliminates dark fringing artifacts at sprite edges during bilinear filtering.
    void ApplySpriteBlendingRGBA(const uint8_t* data, int width, int height, uint8_t* data_out);
} // namespace tex
