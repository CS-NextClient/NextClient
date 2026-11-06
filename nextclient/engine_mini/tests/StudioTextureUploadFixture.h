#pragma once

#include <functional>

#include "graphics/texture_loader.h"

namespace studio_test
{
    struct TextureUploadCallbacks
    {
        std::function<bool(tex::Texture&, const tex::TexIdentifierStr&, TextureFormat, const uint8_t*, int, int, bool, const uint8_t*, int)>
            load;
        std::function<void(tex::Texture&)> release;
    };

    extern TextureUploadCallbacks g_TextureUploadCallbacks;
    extern std::function<void(tex::TextureLoadGuard)> g_MaterialImageLoadCallback;
} // namespace studio_test
