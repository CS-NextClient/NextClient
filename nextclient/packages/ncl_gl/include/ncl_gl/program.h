#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <glad/glad.h>

namespace ncl_gl
{
    inline constexpr int kMaxUniforms = 24;

    // One linked GL program with its resolved uniform locations. loc is indexed by
    // a per-program uniform enum, in the order of the matching name table;
    // ok is true once the program linked (a uniform absent from the program is -1).
    struct Program
    {
        GLuint prog;
        bool ok;
        GLint loc[kMaxUniforms];
    };

    // info_log receives whatever the driver reported, on a build that succeeded as well as
    // on one that failed: a warning about a construct the driver reads its own way is worth
    // seeing before it becomes a report about one machine.
    GLuint CompileShader(GLenum type, const char* src, std::string* info_log = nullptr);
    GLuint LinkProgram(
        const char* vs_src,
        const char* fs_src,
        const char* const* attribs,
        int attrib_count,
        std::string* info_log = nullptr
    );
    void LoadProgram(
        Program& p,
        const char* vs_src,
        const char* fs_src,
        const char* const* names,
        int count,
        const char* const* attribs = nullptr,
        int attrib_count = 0,
        std::string* info_log = nullptr
    );

    // Whether the driver can hand a linked program back as a blob and take one in. The
    // whole feature is optional in GL 3.3, and a driver may also expose it while
    // supporting no binary format at all.
    bool ProgramBinarySupported();

    bool GetProgramBinary(GLuint prog, GLenum& format, std::vector<uint8_t>& out);

    // Rebuilds a program from a blob the driver produced earlier. A rejected blob leaves
    // p.ok false and is not an error: a binary is only ever valid for the driver, and the
    // GPU, that wrote it, and the specification allows rejection at any time.
    void LoadProgramBinary(
        Program& p,
        const void* data,
        size_t size,
        GLenum format,
        const char* const* names,
        int count
    );
}
