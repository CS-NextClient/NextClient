#include "ncl_material/MaterialDoc.h"

namespace ncl_material
{
    namespace
    {
        constexpr char ToLowerAscii(char c)
        {
            return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
        }
    } // namespace

    bool EqualsIgnoreCaseAscii(std::string_view left, std::string_view right)
    {
        if (left.size() != right.size())
        {
            return false;
        }

        for (size_t i = 0; i < left.size(); i++)
        {
            if (ToLowerAscii(left[i]) != ToLowerAscii(right[i]))
            {
                return false;
            }
        }

        return true;
    }

    const ParamValue* Material::FindParam(std::string_view name) const
    {
        for (const Param& param : params)
        {
            if (param.name == name)
            {
                return &param.value;
            }
        }

        return nullptr;
    }

    const OverlaySlot* Material::FindOverlaySlot(std::string_view name) const
    {
        for (const OverlaySlot& slot : overlays)
        {
            if (EqualsIgnoreCaseAscii(slot.name, name))
            {
                return &slot;
            }
        }

        return nullptr;
    }

    const Material* MaterialDoc::FindByTexture(std::string_view texture_name) const
    {
        for (const MaterialEntry& entry : entries)
        {
            if (EqualsIgnoreCaseAscii(entry.texture_name, texture_name))
            {
                return &entry.material;
            }
        }

        return nullptr;
    }
} // namespace ncl_material
