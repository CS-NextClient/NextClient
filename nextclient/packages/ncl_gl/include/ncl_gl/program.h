#pragma once
#include <string>

#include <glad/glad.h>

namespace ncl_gl
{
    inline constexpr int kMaxUniforms = 16;

    // One linked GL program with its resolved uniform locations. loc is indexed by
    // a per-program uniform enum, in the order of the matching name table;
    // ok is true once the program linked (a uniform absent from the program is -1).
    struct Program
    {
        GLuint prog;
        bool ok;
        GLint loc[kMaxUniforms];
    };

    GLuint CompileShader(GLenum type, const char* src, std::string* error_log = nullptr);
    GLuint LinkProgram(
        const char* vs_src,
        const char* fs_src,
        const char* const* attribs,
        int attrib_count,
        std::string* error_log = nullptr
    );
    void LoadProgram(
        Program& p,
        const char* vs_src,
        const char* fs_src,
        const char* const* names,
        int count,
        const char* const* attribs = nullptr,
        int attrib_count = 0,
        std::string* error_log = nullptr
    );
}
