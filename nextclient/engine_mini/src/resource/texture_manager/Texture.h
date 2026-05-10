#pragma once
#include <EASTL/fixed_string.h>
#include "resource/palette_manager/PaletteHandle.h"

namespace tex
{
    using TexIdentifierStr = eastl::fixed_string<char, 160, false>;

    struct Texture
    {
        TexIdentifierStr identifier;
        int width;
        int height;
        bool mipmap;
        int texnum;
        PaletteHandle palette;
    };
}
