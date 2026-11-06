#pragma once

#include <vector>
#include <glad/glad.h>

struct RadarGlObjects
{
    std::vector<GLuint> programs;
    std::vector<GLuint> textures;
    std::vector<GLuint> framebuffers;
    std::vector<GLuint> renderbuffers;

    void Release() const
    {
        glUseProgram(0);
        for (GLuint program : programs)
        {
            glDeleteProgram(program);
        }
        glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
        glDeleteFramebuffers(static_cast<GLsizei>(framebuffers.size()), framebuffers.data());
        glDeleteRenderbuffers(static_cast<GLsizei>(renderbuffers.size()), renderbuffers.data());
    }
};

class RadarGlAllocations
{
private:
    PFNGLCREATESHADERPROC create_shader_ = glad_glCreateShader;
    PFNGLCREATEPROGRAMPROC create_program_ = glad_glCreateProgram;
    PFNGLGENTEXTURESPROC gen_textures_ = glad_glGenTextures;
    PFNGLGENFRAMEBUFFERSPROC gen_framebuffers_ = glad_glGenFramebuffers;
    PFNGLGENRENDERBUFFERSPROC gen_renderbuffers_ = glad_glGenRenderbuffers;
    PFNGLDELETEPROGRAMPROC delete_program_ = glad_glDeleteProgram;
    PFNGLDELETETEXTURESPROC delete_textures_ = glad_glDeleteTextures;
    PFNGLDELETEFRAMEBUFFERSPROC delete_framebuffers_ = glad_glDeleteFramebuffers;
    PFNGLDELETERENDERBUFFERSPROC delete_renderbuffers_ = glad_glDeleteRenderbuffers;
    PFNGLDRAWARRAYSPROC draw_arrays_ = glad_glDrawArrays;
    static inline RadarGlAllocations* current_{};

public:
    RadarGlObjects objects;
    unsigned int delete_calls{};
    unsigned int draws{};
    unsigned int shader_creates{};

    RadarGlAllocations()
    {
        current_ = this;
        glad_glCreateShader = [](GLenum type) -> GLuint {
            ++current_->shader_creates;
            return current_->create_shader_(type);
        };
        glad_glCreateProgram = []() -> GLuint {
            GLuint name = current_->create_program_();
            current_->objects.programs.push_back(name);
            return name;
        };
        glad_glGenTextures = [](GLsizei count, GLuint* names) {
            current_->gen_textures_(count, names);
            current_->objects.textures.insert(current_->objects.textures.end(), names, names + count);
        };
        glad_glGenFramebuffers = [](GLsizei count, GLuint* names) {
            current_->gen_framebuffers_(count, names);
            current_->objects.framebuffers.insert(current_->objects.framebuffers.end(), names, names + count);
        };
        glad_glGenRenderbuffers = [](GLsizei count, GLuint* names) {
            current_->gen_renderbuffers_(count, names);
            current_->objects.renderbuffers.insert(current_->objects.renderbuffers.end(), names, names + count);
        };
        glad_glDeleteProgram = [](GLuint name) {
            ++current_->delete_calls;
            current_->delete_program_(name);
        };
        glad_glDeleteTextures = [](GLsizei count, const GLuint* names) {
            ++current_->delete_calls;
            current_->delete_textures_(count, names);
        };
        glad_glDeleteFramebuffers = [](GLsizei count, const GLuint* names) {
            ++current_->delete_calls;
            current_->delete_framebuffers_(count, names);
        };
        glad_glDeleteRenderbuffers = [](GLsizei count, const GLuint* names) {
            ++current_->delete_calls;
            current_->delete_renderbuffers_(count, names);
        };
        glad_glDrawArrays = [](GLenum mode, GLint first, GLsizei count) {
            ++current_->draws;
            current_->draw_arrays_(mode, first, count);
        };
    }

    ~RadarGlAllocations()
    {
        glad_glCreateShader = create_shader_;
        glad_glCreateProgram = create_program_;
        glad_glGenTextures = gen_textures_;
        glad_glGenFramebuffers = gen_framebuffers_;
        glad_glGenRenderbuffers = gen_renderbuffers_;
        glad_glDeleteProgram = delete_program_;
        glad_glDeleteTextures = delete_textures_;
        glad_glDeleteFramebuffers = delete_framebuffers_;
        glad_glDeleteRenderbuffers = delete_renderbuffers_;
        glad_glDrawArrays = draw_arrays_;
        current_ = nullptr;
    }
};
