#pragma once

#include <string>
#include <vector>

#include <ncl_gl/program.h>

namespace render
{
    // Identity of a cached program: the two source names plus the define set. Two call
    // sites asking for the same triple get the same linked program.
    struct ProgramDesc
    {
        const char* vs_name;
        const char* fs_name;
        const char* defines;              // may be null, injected after the #version line
        const char* const* uniform_names; // may be null
        int uniform_count;
        const char* const* attrib_names;  // may be null
        int attrib_count;
    };

    // Builds the program on first use and caches it. Returns nullptr when the pipeline is
    // unavailable or the program failed to build; a failure is cached as well, so a
    // broken shader is reported once instead of every frame.
    const ncl_gl::Program* Shaders_Get(const ProgramDesc& desc);

    // Builds all of them now, so the first draw that needs one does not stall on a
    // compile. Writes the binary cache once at the end rather than per program.
    void Shaders_WarmUp(const ProgramDesc* descs, int count);

    // Names that have both a <name>.vert and a <name>.frag among the sources the module
    // ships. What a material may name as its program is any of these, so this is the set
    // a start-up build has to cover.
    std::vector<std::string> Shaders_ListPrograms();

    // Drops every cached program, and the blob file with them, so the next Shaders_Get
    // rebuilds from source. The r_reload_shaders command and the r_shader_reload hook
    // that call this live with the studio stage, which also owns the rebuild.
    void Shaders_Reload();

    // Bumped whenever cached programs are dropped, by a device reset or a hot reload. A
    // caller on a per-draw path keeps the resolved pointer and re-asks only when this
    // changes, instead of paying the key lookup every draw.
    uint32_t Shaders_Generation();

    // Reads a file from the module's embedded resources, or from the game directory
    // when r_shader_reload is on. Shared with whatever else ships alongside a program,
    // so a manifest is found the same way its shader is.
    bool Shaders_LoadSource(const char* name, std::string& out);
}
