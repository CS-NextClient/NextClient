#include "MapImageTone.h"

#include <glad/glad.h>

namespace
{
    constexpr float kTexturedShadowLift = 0.17f;

    void BeginColorPass(GLenum source, GLenum destination)
    {
        glActiveTexture(GL_TEXTURE1);
        glDisable(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE0);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(source, destination);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    void DrawColorPass()
    {
        glBegin(GL_TRIANGLE_STRIP);
        glVertex3f(-1.0f, -1.0f, 0.0f);
        glVertex3f(1.0f, -1.0f, 0.0f);
        glVertex3f(-1.0f, 1.0f, 0.0f);
        glVertex3f(1.0f, 1.0f, 0.0f);
        glEnd();
    }
}

void MapImageTone_BrightenTextured()
{
    BeginColorPass(GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Destination alpha confines the lift to map pixels and preserves the clear background.
    glColor4f(kTexturedShadowLift, kTexturedShadowLift, kTexturedShadowLift, kTexturedShadowLift);
    DrawColorPass();
}

void MapImageTone_PremultiplyAlpha()
{
    BeginColorPass(GL_ZERO, GL_DST_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
    DrawColorPass();
}
