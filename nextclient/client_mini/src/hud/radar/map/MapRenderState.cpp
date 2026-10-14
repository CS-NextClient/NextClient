#include "MapRenderState.h"

MapRenderState::MapRenderState()
{
    // Each map pass checks its own errors in the GL context shared with the engine.
    while (glGetError() != GL_NO_ERROR)
    {
    }

    glGetIntegerv(GL_ACTIVE_TEXTURE, &unit_);

    for (int i = 0; i < 2; i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures_[i]);
    }

    glActiveTexture(unit_);

    glGetIntegerv(GL_CURRENT_PROGRAM, &program_);

    split_framebuffers_ = GLAD_GL_VERSION_3_0 || GLAD_GL_ARB_framebuffer_object;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &draw_framebuffer_);

    if (split_framebuffers_)
    {
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer_);
    }

    glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer_);

    glGetIntegerv(GL_MATRIX_MODE, &matrix_mode_);

    glPushAttrib(GL_ALL_ATTRIB_BITS);

    for (int i = 0; i < 2; i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glLoadIdentity();
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
}

MapRenderState::~MapRenderState()
{
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    for (int i = 0; i < 2; i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
    }

    glMatrixMode(matrix_mode_);

    glUseProgram(program_);

    if (split_framebuffers_)
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_framebuffer_);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read_framebuffer_);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, draw_framebuffer_);
    }

    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer_);

    glPopAttrib();

    for (int i = 0; i < 2; i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures_[i]);
    }

    glActiveTexture(unit_);
}
