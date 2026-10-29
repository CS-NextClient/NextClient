#pragma once

#include <string_view>

#include <ncl_material/MaterialDoc.h>

namespace render
{
    class MaterialEditInterface;

    const ncl_material::Material* MaterialEdit_Override(std::string_view model, std::string_view texture_name);
    MaterialEditInterface* MaterialEdit_Get();
    void MaterialEdit_End();
    void MaterialEdit_RegisterCommands();
} // namespace render
