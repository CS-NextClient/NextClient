#include "graphics/preview/PreviewAssetBackend.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <goldsrc/format/wad.h>

#include "console/console.h"
#include "graphics/gl_local.h"

namespace render::preview
{
    namespace
    {
        constexpr int kMipLevels = 4;
        constexpr int kPaletteColors = 256;
    } // namespace

    PreviewAssetBackend::PreviewAssetBackend(LoadFile_t load_file, FreeFile_t free_file) :
        load_file_(load_file),
        free_file_(free_file)
    {}

    void PreviewAssetBackend::BindFiles(LoadFile_t load_file, FreeFile_t free_file)
    {
        load_file_ = load_file;
        free_file_ = free_file;
    }

    uint8_t* PreviewAssetBackend::LoadFile(const char* path, int* length)
    {
        return is_available() ? load_file_(path, 5, length) : nullptr;
    }

    bool PreviewAssetBackend::is_available() const
    {
        return load_file_ != nullptr && free_file_ != nullptr;
    }

    void PreviewAssetBackend::FreeFile(void* bytes)
    {
        free_file_(bytes);
    }

    std::string PreviewAssetBackend::LoadText(const char* path)
    {
        int length = 0;
        uint8_t* data = LoadFile(path, &length);
        if (data == nullptr)
        {
            return {};
        }

        std::string text(reinterpret_cast<const char*>(data), static_cast<size_t>(length));
        FreeFile(data);
        return text;
    }

    int PreviewAssetBackend::UploadMiptex(const char* identifier, uint8_t* lump, int lump_size)
    {
        if (lump_size < static_cast<int>(sizeof(goldsrc::miptex_t)))
        {
            return -1;
        }

        goldsrc::miptex_t header;
        std::memcpy(&header, lump, sizeof(header));

        int wide = static_cast<int>(header.width);
        int tall = static_cast<int>(header.height);

        if (wide <= 0 || tall <= 0)
        {
            return -1;
        }

        uint32_t palette_offset = header.offsets[kMipLevels - 1] + (header.width / 8) * (header.height / 8);

        // Bounded in 64 bits: the offsets are unsigned 32 bit and a corrupt one wraps, so an
        // end past the lump would come out negative and pass as an int comparison.
        int64_t pixels_end = static_cast<int64_t>(header.offsets[0]) + static_cast<int64_t>(wide) * tall;
        int64_t palette_end = static_cast<int64_t>(header.offsets[kMipLevels - 1]) +
                              static_cast<int64_t>(header.width / 8) * (header.height / 8) + 2 + kPaletteColors * 3;

        if (pixels_end > lump_size || palette_end > lump_size)
        {
            return -1;
        }

        return GL_LoadTexture(
            identifier,
            GLT_SYSTEM,
            wide,
            tall,
            lump + header.offsets[0],
            1,
            static_cast<int>(TextureFormat::Opaque),
            lump + palette_offset + 2
        );
    }


    void PreviewAssetBackend::UnloadTexture(const char* identifier)
    {
        GL_UnloadTexture(identifier);
    }

    void PreviewAssetBackend::Report(const char* format, ...)
    {
        char text[2048];
        va_list args;
        va_start(args, format);
        std::vsnprintf(text, sizeof(text), format, args);
        va_end(args);
        Con_Printf("%s", text);
    }
} // namespace render::preview
