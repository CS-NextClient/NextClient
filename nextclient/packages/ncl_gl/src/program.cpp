#include <ncl_gl/program.h>

namespace ncl_gl
{
    namespace
    {
        constexpr int kMaxDrainedErrors = 32;

        void ResolveUniforms(Program& p, const char* const* names, int count)
        {
            for (int i = 0; i < count; i++)
            {
                p.loc[i] = glGetUniformLocation(p.prog, names[i]);
            }
        }

        // The stage label keeps a mixed log readable: on a failed link the collected text
        // starts with the successful compile stages' chatter, and without the labels the
        // error's origin is guesswork.
        template <class Getter, class LogGetter>
        void AppendInfoLog(GLuint object, Getter get_iv, LogGetter get_log, const char* stage, std::string* info_log)
        {
            if (info_log == nullptr)
            {
                return;
            }

            GLint len = 0;
            get_iv(object, GL_INFO_LOG_LENGTH, &len);

            if (len <= 1)
            {
                return;
            }

            std::string log(static_cast<size_t>(len), '\0');
            get_log(object, len, nullptr, log.data());

            info_log->append(stage);
            info_log->append(": ");
            info_log->append(log.c_str());
        }
    }

    GLuint CompileShader(GLenum type, const char* src, std::string* info_log)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint status = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

        const char* stage =
            type == GL_VERTEX_SHADER ? "vertex shader" :
            type == GL_FRAGMENT_SHADER ? "fragment shader" : "shader";

        AppendInfoLog(shader, glGetShaderiv, glGetShaderInfoLog, stage, info_log);

        if (status != GL_TRUE)
        {
            glDeleteShader(shader);
            return 0;
        }

        return shader;
    }

    GLuint LinkProgram(const char* vs_src, const char* fs_src, const char* const* attribs, int attrib_count, std::string* info_log)
    {
        GLuint vs = CompileShader(GL_VERTEX_SHADER, vs_src, info_log);
        GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fs_src, info_log);

        if (vs == 0 || fs == 0)
        {
            if (vs)
            {
                glDeleteShader(vs);
            }

            if (fs)
            {
                glDeleteShader(fs);
            }

            return 0;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);

        // Asked for on every program rather than only on the ones a cache reads back: the
        // hint has to be set before the link, so a program linked without it can never be
        // stored, and the driver keeps a blob for a handful of programs either way.
        if (ProgramBinarySupported())
        {
            glProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
        }

        // Bind the attributes to fixed locations before linking so aPos lands on
        // location 0. GL 2.x compatibility array draws require an enabled array at
        // attribute 0, and a linker free to place aPos elsewhere would leave slot 0
        // empty and drop the batched draw into software processing or nothing at all.
        for (int i = 0; i < attrib_count; i++)
        {
            glBindAttribLocation(program, i, attribs[i]);
        }

        glLinkProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);

        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);

        AppendInfoLog(program, glGetProgramiv, glGetProgramInfoLog, "link", info_log);

        if (linked != GL_TRUE)
        {
            glDeleteProgram(program);
            return 0;
        }

        return program;
    }

    void LoadProgram(
        Program& p,
        const char* vs_src,
        const char* fs_src,
        const char* const* names,
        int count,
        const char* const* attribs,
        int attrib_count,
        std::string* info_log
    )
    {
        p = Program{};

        for (GLint& loc : p.loc)
        {
            loc = -1;
        }

        GLuint prog = LinkProgram(vs_src, fs_src, attribs, attrib_count, info_log);

        if (prog == 0)
        {
            return;
        }

        p.prog = prog;
        ResolveUniforms(p, names, count);
        p.ok = true;
    }

    bool ProgramBinarySupported()
    {
        if (glGetProgramBinary == nullptr || glProgramBinary == nullptr || glProgramParameteri == nullptr)
        {
            return false;
        }

        GLint formats = 0;
        glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &formats);

        return formats > 0;
    }

    bool GetProgramBinary(GLuint prog, GLenum& format, std::vector<uint8_t>& out)
    {
        if (prog == 0 || !ProgramBinarySupported())
        {
            return false;
        }

        GLint length = 0;
        glGetProgramiv(prog, GL_PROGRAM_BINARY_LENGTH, &length);

        if (length <= 0)
        {
            return false;
        }

        out.resize(static_cast<size_t>(length));

        GLsizei written = 0;
        GLenum binary_format = 0;
        glGetProgramBinary(prog, length, &written, &binary_format, out.data());

        if (written <= 0)
        {
            out.clear();
            return false;
        }

        out.resize(static_cast<size_t>(written));
        format = binary_format;

        return true;
    }

    void LoadProgramBinary(
        Program& p,
        const void* data,
        size_t size,
        GLenum format,
        const char* const* names,
        int count
    )
    {
        p = Program{};

        for (GLint& loc : p.loc)
        {
            loc = -1;
        }

        if (data == nullptr || size == 0 || !ProgramBinarySupported())
        {
            return;
        }

        GLuint prog = glCreateProgram();
        glProgramBinary(prog, format, data, static_cast<GLsizei>(size));

        // A rejected blob is reported through the link status rather than through an error,
        // and the driver may also have raised GL_INVALID_ENUM on a format it no longer
        // knows; clearing it here keeps that out of the next unrelated error check. Bounded
        // because a context that has gone away answers every query with an error.
        for (int i = 0; i < kMaxDrainedErrors && glGetError() != GL_NO_ERROR; i++)
        {
        }

        GLint linked = GL_FALSE;
        glGetProgramiv(prog, GL_LINK_STATUS, &linked);

        if (linked != GL_TRUE)
        {
            glDeleteProgram(prog);
            return;
        }

        p.prog = prog;
        ResolveUniforms(p, names, count);
        p.ok = true;
    }
}
