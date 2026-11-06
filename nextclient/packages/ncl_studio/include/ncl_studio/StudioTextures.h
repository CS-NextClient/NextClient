#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <ncl_utils/safe_result.h>

namespace ncl_studio
{
    struct TextureInfo
    {
        std::string name;
        int32_t flags{};
        int32_t width{};
        int32_t height{};
    };

    // Which texture each mesh's skin reference resolves to, per skin family. A mesh names a
    // reference rather than a texture, and the family in force at draw time decides what
    // that reference means; a tool laying anything out per texture has to go through this
    // rather than assume a mesh belongs to one image.
    struct SkinTable
    {
        int32_t families{};
        int32_t references{};
        std::vector<int16_t> values;

        int32_t TextureFor(int32_t family, int32_t reference) const;
    };

    struct StudioTextures
    {
        std::vector<TextureInfo> textures;
        SkinTable skins;
    };

    // Reads owned tables within the supplied cached-block bound without texture pixels.
    // A texture companion uses the same table format as a model with embedded textures.
    ncl_utils::ResultT<StudioTextures> ReadStudioTextures(const void* data, size_t size);
} // namespace ncl_studio
