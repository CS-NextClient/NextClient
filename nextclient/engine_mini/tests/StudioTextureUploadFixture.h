#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "graphics/texture_loader.h"

namespace studio_test
{
    struct MaterialImagePixels
    {
        int width = 1;
        int height = 1;
        std::vector<uint8_t> pixels;
    };

    extern std::map<std::string, MaterialImagePixels> g_MaterialImages;

    struct TextureUploadCallbacks
    {
        std::function<bool(tex::Texture&, const tex::TexIdentifierStr&, TextureFormat, const uint8_t*, int, int, bool, const uint8_t*, int)>
            load;
        std::function<void(tex::Texture&)> release;
    };

    extern TextureUploadCallbacks g_TextureUploadCallbacks;
    extern std::function<void(tex::TextureLoadGuard)> g_MaterialImageLoadCallback;
} // namespace studio_test
