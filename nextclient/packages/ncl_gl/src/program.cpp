#include <ncl_gl/program.h>

namespace ncl_gl
{
    GLuint CompileShader(GLenum type, const char* src, std::string* error_log)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint status = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

        if (status != GL_TRUE)
        {
            if (error_log != nullptr)
            {
                GLint len = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

                if (len > 1)
                {
                    std::string log(static_cast<size_t>(len), '\0');
                    glGetShaderInfoLog(shader, len, nullptr, log.data());
                    error_log->append(log.c_str());
                }
            }

            glDeleteShader(shader);
            return 0;
        }

        return shader;
    }

    GLuint LinkProgram(const char* vs_src, const char* fs_src, const char* const* attribs, int attrib_count, std::string* error_log)
    {
        GLuint vs = CompileShader(GL_VERTEX_SHADER, vs_src, error_log);
        GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fs_src, error_log);

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

        if (linked != GL_TRUE)
        {
            if (error_log != nullptr)
            {
                GLint len = 0;
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

                if (len > 1)
                {
                    std::string log(static_cast<size_t>(len), '\0');
                    glGetProgramInfoLog(program, len, nullptr, log.data());
                    error_log->append(log.c_str());
                }
            }

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
        std::string* error_log
    )
    {
        p = Program{};

        GLuint prog = LinkProgram(vs_src, fs_src, attribs, attrib_count, error_log);

        if (prog == 0)
        {
            return;
        }

        p.prog = prog;

        for (int i = 0; i < count; i++)
        {
            p.loc[i] = glGetUniformLocation(prog, names[i]);
        }

        p.ok = true;
    }
}
