#pragma once

#include <string>
#include <string_view>

#include <ncl_material/ProgramManifest.h>

namespace render
{
    // Publishes only embedded manifests with matching names, embedded shader pairs and valid renderer contracts.
    // A missing or rejected manifest returns nullptr; rejection is reported once.
    const ncl_material::ProgramManifest* Manifest_Get(const std::string& program);

    const ncl_material::ParamValue* Manifest_GetParam(const ncl_material::Material* material, std::string_view name);
} // namespace render
