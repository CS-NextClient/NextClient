#include "ncl_material/MaterialResolve.h"

namespace ncl_material
{
    namespace
    {
        void AddFlag(Material& material, const char* name, int flags, int bit)
        {
            ParamValue value;
            value.type = ParamType::Scalar;
            value.scalar = (flags & bit) != 0 ? 1.0f : 0.0f;

            material.params.push_back(Param{name, value});
        }
    } // namespace

    Material MakeImplicitMaterial(int studio_flags)
    {
        Material material;
        material.program = kLegacyProgram;

        // All five always, set or not: a caller reading the material should not have to
        // distinguish "flag is off" from "the model did not mention it".
        AddFlag(material, "flatshade", studio_flags, kFlagFlatShade);
        AddFlag(material, "chrome", studio_flags, kFlagChrome);
        AddFlag(material, "fullbright", studio_flags, kFlagFullBright);
        AddFlag(material, "additive", studio_flags, kFlagAdditive);
        AddFlag(material, "masked", studio_flags, kFlagMasked);

        return material;
    }

    ResolvedMaterial Resolve(const MaterialSources& sources, std::string_view texture_name, int studio_flags)
    {
        if (sources.instance != nullptr)
        {
            return ResolvedMaterial{*sources.instance, MaterialOrigin::Instance};
        }

        if (sources.model != nullptr)
        {
            if (const Material* material = sources.model->FindByTexture(texture_name))
            {
                return ResolvedMaterial{*material, MaterialOrigin::ModelDocument};
            }
        }

        return ResolvedMaterial{MakeImplicitMaterial(studio_flags), MaterialOrigin::Implicit};
    }
} // namespace ncl_material
