#pragma once

#include <string_view>

#include "MaterialDoc.h"

namespace ncl_material
{
    // Mirrors the STUDIO_NF_* bits of the studio format. Declared here rather than
    // included so the package stays free of engine headers.
    inline constexpr int kFlagFlatShade = 0x0001;
    inline constexpr int kFlagChrome = 0x0002;
    inline constexpr int kFlagFullBright = 0x0004;
    inline constexpr int kFlagAdditive = 0x0020;
    inline constexpr int kFlagMasked = 0x0040;

    // The program every model gets when nothing else describes it, so that a model with
    // no authored material is still described by a material rather than by a special
    // case somewhere in the renderer.
    inline constexpr const char* kLegacyProgram = "studio_legacy";

    Material MakeImplicitMaterial(int studio_flags);

    enum class MaterialOrigin
    {
        Instance,
        ModelDocument,
        Implicit
    };

    struct MaterialSources
    {
        // Assigned to one entity by the server, outranks everything.
        const Material* instance = nullptr;

        // Shipped next to the model by whoever made it.
        const MaterialDoc* model = nullptr;
    };

    struct ResolvedMaterial
    {
        Material material;
        MaterialOrigin origin = MaterialOrigin::Implicit;
    };

    // Fixed order: instance, then the model's own file, then the material implied by the
    // studio flags. Returns by value because this runs when a model is prepared, not per
    // draw.
    ResolvedMaterial Resolve(const MaterialSources& sources, std::string_view texture_name, int studio_flags);
}
