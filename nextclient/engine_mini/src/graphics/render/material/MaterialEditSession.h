#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <ncl_material/MaterialDoc.h>

namespace render
{
    class MaterialEditor;

    class MaterialEditSession
    {
        friend class MaterialEditor;

    private:
        struct ParamSnapshot
        {
            bool present{};
            size_t index{};
            ncl_material::ParamValue value;
        };

        bool active{};
        int entity_index{};
        std::string model;
        std::vector<ncl_material::MaterialEntry> entries;
        std::string preview_param;
        std::vector<ParamSnapshot> preview_values;

    public:
        const ncl_material::Material* FindOverride(std::string_view model, std::string_view texture_name) const;
    };
} // namespace render
