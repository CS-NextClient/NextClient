#include <SDL_opengl.h>
#include <SDL_video.h>
#include <optick.h>
#include <imageinfo.hpp>
#include <stb/stb_image.h>

#include "common/memory/ScratchArena.h"
#include "common/filesystem.h"
#include "common/sys_dll.h"
#include "console/console.h"
#include "graphics/gl_draw.h"
#include "graphics/texture_processing.h"
#include "resource/palette_manager/PaletteManager.h"
#include "resource/texture_manager/TextureManager.h"


using namespace imageinfo;

namespace tex
{
    static ScratchAllocator<32 * 1024 * 1024> g_TempAllocator;

    static std::tuple<GLenum, GLenum> GL_GetFormatAndInternalFormat(TextureFormat format)
    {
        GLenum gl_format;
        GLenum gl_internal_format;

        switch (format)
        {
        case TextureFormat::Opaque:
            gl_format = GL_RGBA;
            gl_internal_format = GL_RGBA8;
            break;

        case TextureFormat::Alpha:
        case TextureFormat::AlphaGradient:
            gl_format = GL_RGBA;
            gl_internal_format = GL_RGBA8;
            break;

        case TextureFormat::Grayscale:
            gl_format = GL_LUMINANCE;
            gl_internal_format = GL_LUMINANCE8;
            break;

        case TextureFormat::RGBA:
            gl_format = GL_RGBA;
            gl_internal_format = GL_RGBA8;
            break;

        default:
            Sys_Error("GL_Upload32: unknown texture type");
            break;
        }

        return {gl_format, gl_internal_format};
    }

    static void ResampleTexture(
        const uint8_t* data,
        int width,
        int height,
        TextureFormat format,
        uint8_t* data_out,
        int scaled_width,
        int scaled_height
    )
    {
        if (format == TextureFormat::Grayscale)
        {
            ResampleGrayscale(data, width, height, data_out, scaled_width, scaled_height);
        }
        else if (IsIndexedTexture(format))
        {
            ResamplePointIndexed(data, width, height, data_out, scaled_width, scaled_height);
        }
        else
        {
            ResampleRGBA(data, width, height, data_out, scaled_width, scaled_height);
        }
    }

    static void GenerateAndUploadMipmaps(
        uint8_t* work_buf,
        const uint8_t* data,
        int width,
        int height,
        GLenum gl_format,
        GLenum gl_internal_format
    )
    {
        OPTICK_EVENT();

        if (data != work_buf)
        {
            V_memcpy(work_buf, data, width * height * 4);
        }

        int mip_level = 0;
        int mip_width = width;
        int mip_height = height;

        while (mip_width > 1 || mip_height > 1)
        {
            MipMapRGBA(work_buf, mip_width, mip_height, work_buf);

            mip_width = std::max(1, mip_width >> 1);
            mip_height = std::max(1, mip_height >> 1);

            qglTexImage2D(GL_TEXTURE_2D, ++mip_level, gl_internal_format, mip_width, mip_height, 0, gl_format, GL_UNSIGNED_BYTE, work_buf);
        }
    }

    static void SetTextureFilters(bool mipmap, int filter)
    {
        int filter_min = mipmap ? gl_filter_min : filter;
        int filter_max = mipmap ? gl_filter_max : filter;

        qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_min);
        qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_max);
        qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value);
    }

    static bool LoadTGA(const char* file, uint8_t* rgba_out, uint32_t size, int* width_out, int* height_out)
    {
        const uint32_t width_max = kTextureMaxSize;
        const uint32_t height_max = kTextureMaxSize;
        const uint32_t file_size_limit = size + 18 + 256 * 4 + 255; // TGA header (18) + max colormap (256*4) + max image ID (255)

        int file_len = 0;
        FileHandle_t file_handle = nullptr;
        unsigned char* file_buffer = COM_LoadFileLimit(file, 0, file_size_limit, &file_len, &file_handle);

        if (file_buffer == nullptr)
        {
            Con_Printf("Failed to load file: %s.\n", file);
            return false;
        }

        // if (!FS_EndOfFile(file_buffer))
        // {
        //     Con_Printf("Failed to load file: %s. File is too large.\n", file);
        //     FS_Close(file_handle);
        //     return false;
        // }

        ImageInfo image_info = imageinfo::parse<RawDataReader>(RawData(file_buffer, file_len), {kFormatTga}, true);

        if (!image_info.ok())
        {
            Con_Printf("Failed to parse image info: %s. Error: %s\n", file, image_info.error());
            FS_Close(file_handle);
            return false;
        }

        int width = image_info.size().width;
        int height = image_info.size().height;

        if (width > width_max || height > height_max)
        {
            Con_Printf("Image size is too large %s. Actual: %dx%d, Max: %dx%d\n", file, width, height, width_max, height_max);
            FS_Close(file_handle);
            return false;
        }

        int components;
        stbi_uc* image = stbi_load_from_memory(file_buffer, file_len, &width, &height, &components, STBI_rgb_alpha);

        if (image == nullptr)
        {
            Con_Printf("Failed to parse image: %s\n", stbi_failure_reason());
            FS_Close(file_handle);
            return false;
        }

        V_memcpy(rgba_out, image, width * height * 4);
        *width_out = width;
        *height_out = height;

        stbi_image_free(image);
        FS_Close(file_handle);

        return true;
    }

    bool LoadTextureFromFile(Texture& texture, const TexIdentifierStr& identifier, const char* filename, bool mipmap, int filter)
    {
        const int array_size = kTextureMaxSize * kTextureMaxSize * 4;

        ScratchArena arena(g_TempAllocator);
        uint8_t* data = arena.AllocateArray<uint8_t>(array_size);

        char new_filename[MAX_OSPATH];
        V_sprintf_safe(new_filename, "gfx/%s.tga", filename);

        int width, height;
        if (LoadTGA(new_filename, data, array_size, &width, &height))
        {
            return LoadTexture(texture, identifier, TextureFormat::RGBA, data, width, height, mipmap, nullptr, filter);
        }

        return false;
    }

    bool LoadTexture(
        Texture& texture,
        const TexIdentifierStr& identifier,
        TextureFormat format,
        const uint8_t* data,
        int width,
        int height,
        bool mipmap,
        const uint8_t* palette,
        int filter
    )
    {
        OPTICK_EVENT();

        if (width > kTextureMaxSize || height > kTextureMaxSize)
        {
            Con_DPrintf(
                ConLogType::Error,
                "Texture %s is too big: %dx%d, max: %dx%d",
                identifier.c_str(),
                width,
                height,
                kTextureMaxSize,
                kTextureMaxSize
            );
            return false;
        }

        if (g_modfuncs.m_pfnTextureLoad)
        {
            g_modfuncs.m_pfnTextureLoad(identifier.c_str(), width, height, (char*)data);
        }

        // --- Texture metadata ---

        texture.identifier.assign(identifier);
        texture.width = width;
        texture.height = height;
        texture.mipmap = mipmap;
        texture.texnum = GL_GenTexture();
        texture.palette = PaletteHandle::Invalid();

        GL_Bind(texture.texnum);
        *p_currenttexture = -1;

        // --- POT dimensions ---

        int pot_width = width;
        int pot_height = height;
        bool needs_pot = false;

        if (!bSupportsNPOTTextures)
        {
            GetAdjustedTexturePOTSize(width, height, &pot_width, &pot_height);
            needs_pot = (width != pot_width || height != pot_height) && pot_width > 0 && pot_height > 0;
        }

        // --- HW palette fast path (no RGBA conversion) ---

        bool can_use_hw_palette = !mipmap && format == TextureFormat::Opaque && qglColorTableEXT && gl_palette_tex.value;
        if (can_use_hw_palette)
        {
            ScratchArena arena(g_TempAllocator);

            if (needs_pot)
            {
                uint8_t* pot_data = arena.AllocateArray<uint8_t>(pot_width * pot_height);
                ResamplePointIndexed(data, width, height, pot_data, pot_width, pot_height);

                data = pot_data;
                width = pot_width;
                height = pot_height;
            }

            texture.palette = g_PaletteManagerGlob.Load(palette);

            qglTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, width, height, GL_FALSE, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, data);
            SetTextureFilters(false, filter);

            return true;
        }

        // --- Working buffer ---

        int final_w = needs_pot ? pot_width : width;
        int final_h = needs_pot ? pot_height : height;

        bool is_indexed = IsIndexedTexture(format);
        bool needs_work_buf = needs_pot || is_indexed || mipmap;

        ScratchArena arena(g_TempAllocator);

        int buf_bpe = (format == TextureFormat::Grayscale) ? 1 : 4;
        uint8_t* work_buf = needs_work_buf ? arena.AllocateArray<uint8_t>(final_w * final_h * buf_bpe) : nullptr;

        // --- Indexed texture pipeline: POT resample (index space) -> unpack to RGBA -> sprite blending ---

        if (is_indexed)
        {
            // POT resampling is done in index space (1 bpp, point sampling) to preserve
            // exact palette indices. A separate buffer is used because work_buf will be
            // the target of the RGBA unpack below (4 bpp), and in-place would alias.
            const uint8_t* indexed_data = data;
            if (needs_pot)
            {
                uint8_t* pot_indexed = arena.AllocateArray<uint8_t>(final_w * final_h);
                ResamplePointIndexed(data, width, height, pot_indexed, final_w, final_h);
                indexed_data = pot_indexed;
                width = final_w;
                height = final_h;
            }

            TextureFormat original_format = format;

            if (!TryUnpackTextureToRGBA(indexed_data, format, width, height, palette, work_buf))
            {
                LOG(ERROR) << "Failed to unpack texture to RGBA";
                return false;
            }

            data = work_buf;
            format = TextureFormat::RGBA;

            // In indexed alpha textures (Alpha, AlphaGradient), transparent pixels are
            // unpacked as RGBA(0,0,0,0). During bilinear filtering, the black RGB of these
            // pixels bleeds into neighboring opaque edges, causing visible dark fringing.
            // ApplySpriteBlendingRGBA replaces the RGB of fully-transparent pixels with
            // the average RGB of their opaque 3x3 neighbors (via BoxFilter3x3RGBA), while
            // keeping alpha=0. This eliminates the dark halo artifact.
            if (gl_spriteblend->value > 0.0 && IsTextureAlpha(original_format))
            {
                ApplySpriteBlendingRGBA(data, width, height, work_buf);
            }
        }
        // --- Non-indexed: POT resample directly in pixel space (RGBA / Grayscale) ---
        else if (needs_pot)
        {
            ResampleTexture(data, width, height, format, work_buf, final_w, final_h);
            data = work_buf;
            width = final_w;
            height = final_h;
        }

        // --- Upload level 0 ---

        auto [gl_format, gl_internal_format] = GL_GetFormatAndInternalFormat(format);
        qglTexImage2D(GL_TEXTURE_2D, 0, gl_internal_format, width, height, 0, gl_format, GL_UNSIGNED_BYTE, data);

        // --- Mipmaps ---

        if (mipmap)
        {
            GenerateAndUploadMipmaps(work_buf, data, width, height, gl_format, gl_internal_format);
        }

        // --- Filter parameters ---

        SetTextureFilters(mipmap, filter);

        return true;
    }

    void UnloadTexture(Texture& texture)
    {
        OPTICK_EVENT();

        if (texture.palette.IsValid())
        {
            g_PaletteManagerGlob.Release(texture.palette);
            texture.palette = PaletteHandle::Invalid();
        }

        qglDeleteTextures(1, (GLuint*)&texture.texnum);
    }
} // namespace tex
