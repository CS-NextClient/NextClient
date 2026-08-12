#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <ncl_utils/safe_result.h>

#include "MaterialDoc.h"

namespace ncl_material
{
    inline constexpr int kPresetVersion = 1;

    // Ceilings on everything a server can inflate. Exceeding one rejects the whole preset:
    // a truncated preset draws something the server did not describe, and a server cannot
    // tell which of its materials survived.
    struct PresetBudget
    {
        size_t max_materials = 128;

        // Distinct texture files one preset may pull in. This bounds the count, not the
        // memory: the byte ceiling belongs to the texture cache, which is what actually
        // knows the resolutions.
        size_t max_textures = 64;

        size_t max_path_length = 96;
    };

    struct PresetMaterial
    {
        // What a plugin names to put this on an entity. Unique within the preset; a name
        // rather than a number because a plugin is written by a person, and a number in
        // that position is a thing to be got wrong silently.
        std::string name;

        // Which texture of the model this dresses. A material belongs to a texture name,
        // so this is also what identifies the material on an entity that wears several.
        std::string texture_name;

        Material material;
    };

    struct MaterialPreset
    {
        int version = kPresetVersion;
        std::vector<PresetMaterial> materials;

        const PresetMaterial* FindByName(std::string_view name) const;
    };

    // A path from a server is constrained rather than trusted: relative, no traversal,
    // no device or root anchor. Says nothing about what the file is for.
    bool IsAdmissibleRelativePath(std::string_view path, size_t max_length);

    // The above, plus an extension the texture loader actually reads.
    bool IsAdmissibleTexturePath(std::string_view path, size_t max_length);

    // What the client's upload path will accept of a material image. Declared here rather
    // than beside that path, because the offline tool exists to answer for the client and a
    // rule it restated in its own words was a rule the two disagreed on.
    //
    // The ceiling is on the number of pixels and not on either side, so a long strip is
    // admissible where a square of the same area is; and only these two formats are decoded,
    // whatever a file is named.
    inline constexpr int64_t kMaxImagePixels = 1024ll * 1024ll;
    inline constexpr const char* kImageFormats = "TGA or PNG";

    ncl_utils::ResultT<MaterialPreset> ParseMaterialPreset(std::string_view json, const PresetBudget& budget);

    // One material as a preset would carry it, for an author to paste into one. Written
    // here rather than by whoever asks, so the shape stays the one the parser above reads.
    std::string WritePresetMaterial(const PresetMaterial& material);
}
