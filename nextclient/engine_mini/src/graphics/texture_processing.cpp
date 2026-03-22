#include "engine.h"
#include "texture_processing.h"

#include <algorithm>

#include <magic_enum/magic_enum.hpp>
#include <optick.h>

#include "console/console.h"
#include "graphics/gl_local.h"
#include "graphics/gl_draw.h"

namespace tex
{
    void GetAdjustedTexturePOTSize(int width, int height, int* width_out, int* height_out)
    {
        OPTICK_EVENT();

        int max_size = std::max(128, (int)gl_max_size.value);
        int round_down = (int)gl_round_down.value;
        int picmip = (int)gl_picmip.value;

        auto next_power_of_two = [](int value) {
            int result = 1;
            while (result < value)
            {
                result <<= 1;
            }

            return result;
        };

        auto apply_round_down = [round_down](int original, int scaled) {
            if (round_down > 0 && original < scaled && (round_down == 1 || (scaled - original) > (scaled >> round_down)))
            {
                return scaled >> 1;
            }

            return scaled;
        };

        int scaled_width = apply_round_down(width, next_power_of_two(width));
        int scaled_height = apply_round_down(height, next_power_of_two(height));

        if (width_out)
        {
            *width_out = std::min(scaled_width >> picmip, max_size);
        }

        if (height_out)
        {
            *height_out = std::min(scaled_height >> picmip, max_size);
        }
    }

    void ApplyGammaToPalette(const uint8_t* data, uint8_t* data_out, int count)
    {
        OPTICK_EVENT();

        for (int i = 0; i < count; ++i)
        {
            data_out[i] = texgammatable[data[i]];
        }
    }

    TextureFormat ConvertIndexedToRGBA(
        const uint8_t* indices,
        int count,
        const uint8_t* palette,
        TextureFormat format,
        bool dither,
        uint8_t* data_out
    )
    {
        OPTICK_EVENT();

        const uint32_t alpha_mask = 0xFFu << 24;

        uint32_t* out_4byte = (uint32_t*)data_out;

        auto pack_rgb = [](const uint8_t* pal) -> uint32_t { return pal[0] | (pal[1] << 8) | (pal[2] << 16); };

        switch (format)
        {
        case TextureFormat::Opaque:
            {
                if ((count & 3) != 0)
                {
                    Con_DPrintf(ConLogType::Error, "ConvertIndexedToRGBA: invalid count: %d\n", count);
                    return TextureFormat::Invalid;
                }

                if (!dither)
                {
                    for (int i = 0; i < count; ++i)
                    {
                        int pal_index = indices[i] * 3;
                        out_4byte[i] = pack_rgb(&palette[pal_index]) | alpha_mask;
                    }
                }
                else
                {
                    for (int i = 0; i < count; ++i)
                    {
                        const uint8_t* pal = &palette[3 * indices[i]];

                        uint8_t r = pal[0] | (pal[0] >> 6);
                        uint8_t g = pal[1] | (pal[1] >> 6);
                        uint8_t b = pal[2] | (pal[2] >> 6);

                        out_4byte[i] = r | (g << 8) | (b << 16) | alpha_mask;
                    }
                }
                break;
            }

        case TextureFormat::Alpha:
            {
                bool all_opaque = true;
                for (int i = 0; i < count; ++i)
                {
                    int index = indices[i];
                    if (index == 255)
                    {
                        all_opaque = false;
                        out_4byte[i] = 0;
                        continue;
                    }

                    int pal_index = index * 3;
                    out_4byte[i] = pack_rgb(&palette[pal_index]) | alpha_mask;
                }

                if (all_opaque)
                {
                    format = TextureFormat::Opaque;
                }
                break;
            }

        case TextureFormat::AlphaGradient:
            {
                const int base_index = 256 * 3 - 3;
                uint32_t base_color = pack_rgb(&palette[base_index]);

                for (int i = 0; i < count; ++i)
                {
                    out_4byte[i] = base_color | ((uint32_t)indices[i] << 24);
                }
                break;
            }

        case TextureFormat::RGBA:
            {
                for (int i = 0; i < count; ++i)
                {
                    int pal_index = indices[i] * 3;
                    uint32_t base_color = pack_rgb(&palette[pal_index]);
                    out_4byte[i] = base_color | ((uint32_t)indices[i] << 24);
                }
                break;
            }
        default:
            return TextureFormat::Invalid;
        }

        return format;
    }

    void ResamplePointIndexed(const uint8_t* data, int width, int height, uint8_t* data_out, int scaled_width, int scaled_height)
    {
        OPTICK_EVENT();

        uint32_t src_x_step = ((uint32_t)width << 16) / scaled_width;
        uint32_t src_y_step = ((uint32_t)height << 16) / scaled_height;
        uint32_t src_y_fraction = src_y_step >> 2;

        int rows_left = scaled_height;
        while (rows_left-- > 0)
        {
            if (scaled_width > 0)
            {
                uint32_t src_x = src_x_step >> 2;
                int columns_left = scaled_width;

                while (columns_left-- > 0)
                {
                    *data_out++ = data[src_x >> 16];
                    src_x += src_x_step;
                }
            }

            uint32_t src_y_advance = src_y_step + src_y_fraction;
            src_y_fraction = (uint16_t)src_y_advance;
            data += width * (src_y_advance >> 16);
        }
    }

    int MipMapRGBA(const uint8_t* data, int width, int height, uint8_t* data_out)
    {
        OPTICK_EVENT();

        int row_bytes = width << 2;
        int out_height = height >> 1;

        for (int y = 0; y < out_height; ++y, data += row_bytes * 2)
        {
            const uint8_t* in_row = data;
            const uint8_t* next_row = data + row_bytes;

            for (int x = 0; x < row_bytes; x += 8, in_row += 8, next_row += 8, data_out += 4)
            {
                data_out[0] = (in_row[0] + in_row[4] + next_row[0] + next_row[4]) >> 2;
                data_out[1] = (in_row[1] + in_row[5] + next_row[1] + next_row[5]) >> 2;
                data_out[2] = (in_row[2] + in_row[6] + next_row[2] + next_row[6]) >> 2;
                data_out[3] = (in_row[3] + in_row[7] + next_row[3] + next_row[7]) >> 2;
            }
        }

        return out_height;
    }

    int BoxFilter3x3RGBA(const uint8_t* data, int width, int height, int x, int y, uint8_t* data_out)
    {
        OPTICK_EVENT();

        int sum_r = 0;
        int sum_g = 0;
        int sum_b = 0;
        int count = 0;

        int start_x = x - 1;
        int start_y = y - 1;

        for (int offset_x = 0; offset_x < 3; ++offset_x)
        {
            int sample_x = start_x + offset_x;

            for (int offset_y = 0; offset_y < 3; ++offset_y)
            {
                int sample_y = start_y + offset_y;

                if (sample_x < 0 || sample_x >= width || sample_y < 0 || sample_y >= height)
                {
                    continue;
                }

                const uint8_t* pixel = data + 4 * (sample_x + sample_y * width);
                if (pixel[3])
                {
                    sum_r += pixel[0];
                    sum_g += pixel[1];
                    sum_b += pixel[2];
                    ++count;
                }
            }
        }

        if (count == 0)
        {
            count = 1;
        }

        data_out[0] = sum_r / count;
        data_out[1] = sum_g / count;
        data_out[2] = sum_b / count;
        data_out[3] = 0;

        return sum_b / count;
    }

    void ResampleRGBA(const uint8_t* data, int width, int height, uint8_t* data_out, int scaled_width, int scaled_height)
    {
        OPTICK_EVENT();

        if (scaled_width <= 0 || scaled_height <= 0)
        {
            return;
        }

        uint32_t step_x = ((unsigned int)width << 16) / scaled_width;
        uint32_t sample_x0 = step_x >> 2;
        uint32_t sample_x1 = 3 * (step_x >> 2);
        uint32_t x_offsets0[kTextureMaxSize];
        uint32_t x_offsets1[kTextureMaxSize];

        for (int x = 0; x < scaled_width; ++x)
        {
            x_offsets0[x] = 4 * (sample_x0 >> 16);
            sample_x0 += step_x;
        }

        for (int x = 0; x < scaled_width; ++x)
        {
            x_offsets1[x] = 4 * (sample_x1 >> 16);
            sample_x1 += step_x;
        }

        int src_row_bytes = width * 4;
        int dst_row_bytes = scaled_width * 4;
        double height_scale = (double)height / (double)scaled_height;

        for (int y = 0; y < scaled_height; ++y)
        {
            int row0 = (int)((y + 0.25) * height_scale);
            int row1 = (int)((y + 0.75) * height_scale);
            const uint8_t* in_row0 = data + row0 * src_row_bytes;
            const uint8_t* in_row1 = data + row1 * src_row_bytes;
            uint8_t* out_pixel = data_out + y * dst_row_bytes;

            for (int x = 0; x < scaled_width; ++x)
            {
                uint32_t offset0 = x_offsets0[x];
                uint32_t offset1 = x_offsets1[x];

                out_pixel[0] = (in_row0[offset0] + in_row0[offset1] + in_row1[offset0] + in_row1[offset1]) >> 2;
                out_pixel[1] = (in_row0[offset0 + 1] + in_row0[offset1 + 1] + in_row1[offset0 + 1] + in_row1[offset1 + 1]) >> 2;
                out_pixel[2] = (in_row0[offset0 + 2] + in_row0[offset1 + 2] + in_row1[offset0 + 2] + in_row1[offset1 + 2]) >> 2;
                out_pixel[3] = (in_row0[offset0 + 3] + in_row0[offset1 + 3] + in_row1[offset0 + 3] + in_row1[offset1 + 3]) >> 2;
                out_pixel += 4;
            }
        }
    }


    void ResampleGrayscale(const uint8_t* data, int width, int height, uint8_t* data_out, int scaled_width, int scaled_height)
    {
        OPTICK_EVENT();

        if (scaled_width <= 0 || scaled_height <= 0)
        {
            return;
        }

        uint32_t step_x = ((unsigned int)width << 16) / scaled_width;
        uint32_t sample_x0 = step_x >> 2;
        uint32_t sample_x1 = 3 * (step_x >> 2);

        uint32_t x_offsets0[kTextureMaxSize];
        uint32_t x_offsets1[kTextureMaxSize];

        for (int x = 0; x < scaled_width; ++x)
        {
            x_offsets0[x] = sample_x0 >> 16;
            sample_x0 += step_x;
        }

        for (int x = 0; x < scaled_width; ++x)
        {
            x_offsets1[x] = sample_x1 >> 16;
            sample_x1 += step_x;
        }

        double height_scale = (double)height / (double)scaled_height;

        for (int y = 0; y < scaled_height; ++y)
        {
            int row0 = (int)((y + 0.25) * height_scale);
            int row1 = (int)((y + 0.75) * height_scale);
            const uint8_t* in_row0 = data + width * row0;
            const uint8_t* in_row1 = data + width * row1;

            for (int x = 0; x < scaled_width; ++x)
            {
                uint32_t offset0 = x_offsets0[x];
                uint32_t offset1 = x_offsets1[x];

                data_out[x] = (in_row0[offset0] + in_row0[offset1] + in_row1[offset0] + in_row1[offset1]) >> 2;
            }

            data_out += scaled_width;
        }
    }

    bool TryUnpackTextureToRGBA(const uint8_t* data, TextureFormat format, int width, int height, const uint8_t* palette, uint8_t* data_out)
    {
        OPTICK_EVENT();

        if (!palette)
        {
            Con_DPrintf(ConLogType::Error, "TryUnpackTextureToRGBA: No palette\n");
            return false;
        }

        if (!IsIndexedTexture(format))
        {
            Con_DPrintf(ConLogType::Error, "TryUnpackTextureToRGBA: Invalid texture type: %s\n", magic_enum::enum_name(format).data());
            return false;
        }

        uint8_t gamma_palette[256 * 3];
        ApplyGammaToPalette(palette, gamma_palette, sizeof(gamma_palette));

        bool dither = gl_dither_cvar->value != 0.0;
        TextureFormat upload_type = ConvertIndexedToRGBA(data, width * height, gamma_palette, format, dither, data_out);

        if (upload_type == TextureFormat::Invalid)
        {
            Con_Printf("TryUnpackTextureToRGBA: GL_ConvertIndexedToRGBA can't convert texture\n");
            return false;
        }

        return true;
    }

    void ApplySpriteBlendingRGBA(const uint8_t* data, int width, int height, uint8_t* data_out)
    {
        OPTICK_EVENT();

        int texels = width * height;

        for (int index = 0; index < texels; ++index)
        {
            const uint32_t* rgba_pixel_ptr = (const uint32_t*)(data) + index;

            if (*rgba_pixel_ptr == 0)
            {
                BoxFilter3x3RGBA(data, width, height, index % width, index / width, data_out + index * 4);
            }
        }
    }
} // namespace tex
