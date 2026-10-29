#pragma once

#include <glad/glad.h>

class ShaderBindingTrace
{
private:
    PFNGLGETUNIFORMLOCATIONPROC uniform_;
    PFNGLGETUNIFORMBLOCKINDEXPROC block_;
    inline static ShaderBindingTrace* active_{};
    unsigned int uniforms_{};
    unsigned int blocks_{};

public:
    ShaderBindingTrace() :
        uniform_(glGetUniformLocation),
        block_(glGetUniformBlockIndex)
    {
        active_ = this;
        glGetUniformLocation = Uniform;
        glGetUniformBlockIndex = Block;
    }

    ~ShaderBindingTrace()
    {
        glGetUniformLocation = uniform_;
        glGetUniformBlockIndex = block_;
        active_ = nullptr;
    }

    unsigned int get_uniforms() const
    {
        return uniforms_;
    }

    unsigned int get_blocks() const
    {
        return blocks_;
    }

private:
    static GLint APIENTRY Uniform(GLuint program, const GLchar* name)
    {
        ++active_->uniforms_;
        return active_->uniform_(program, name);
    }

    static GLuint APIENTRY Block(GLuint program, const GLchar* name)
    {
        ++active_->blocks_;
        return active_->block_(program, name);
    }
};
