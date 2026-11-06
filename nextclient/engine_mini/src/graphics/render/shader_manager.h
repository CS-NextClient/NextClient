#pragma once

#include <string>
#include <vector>

#include <ncl_gl/program.h>

namespace render
{
    // Cache identity includes both source names, defines, and the ordered uniform and attribute name tables.
    struct ProgramDesc
    {
        const char* vs_name;
        const char* fs_name;
        const char* defines; // may be null, injected after the #version line
        const char* const* uniform_names; // may be null
        int uniform_count;
        const char* const* attrib_names; // may be null
        int attrib_count;
    };

    // Builds the program on first use and caches it. Returns nullptr when the pipeline is
    // unavailable or the program failed to build; a failure is cached as well, so a
    // broken shader is reported once instead of every frame.
    const ncl_gl::Program* Shaders_Get(const ProgramDesc& desc);

    // Builds all of them now, so the first draw that needs one does not stall on a
    // compile.
    void Shaders_WarmUp(const ProgramDesc* descs, int count);

    // Names that have both a <name>.vert and a <name>.frag among the sources the module
    // ships. What a material may name as its program is any of these, so this is the set
    // a start-up build has to cover.
    std::vector<std::string> Shaders_ListPrograms();

    // Changes whenever a device reset invalidates cached program pointers.
    uint32_t Shaders_Generation();
    void Shaders_Clear();

    // Shader sources, includes and manifests come only from the module's embedded resources.
    bool Shaders_LoadSource(const char* name, std::string& out);
} // namespace render
