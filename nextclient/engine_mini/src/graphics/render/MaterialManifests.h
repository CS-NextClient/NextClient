#pragma once

#include <string>

#include <ncl_material/ProgramManifest.h>

namespace render
{
    // The manifest a program ships beside its sources, as <program>.json. Returns nullptr
    // when the program declares none or the file does not parse; the parse error is
    // reported once.
    const ncl_material::ProgramManifest* Manifest_Get(const std::string& program);

    // Dropped along with the programs, so a hot reload picks up an edited manifest too.
    void Manifests_Clear();
}
