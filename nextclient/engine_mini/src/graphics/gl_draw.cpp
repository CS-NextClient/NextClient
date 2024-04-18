#include "../engine.h"
#include <optick.h>
#include "../graphics/gl_local.h"

GLenum oldtarget;
int cnttextures[2] = { -1, -1 }; // cached

GLint scissor_x;
GLint scissor_y;
GLsizei scissor_width;
GLsizei scissor_height;
qboolean giScissorTest;

static void AdjustSubRect(mspriteframe_t* pFrame, float* pfLeft, float* pfRight, float* pfTop, float* pfBottom, int* pw, int* ph, const wrect_t* prcSubRect)
{
    OPTICK_EVENT();

    if (!prcSubRect)
        return;

    int left = std::max(0, prcSubRect->left);
    int right = std::min(*pw, prcSubRect->right);
    int top = std::max(0, prcSubRect->top);
    int bottom = std::min(*ph, prcSubRect->bottom);

    if (left >= right || top >= bottom)
        return;

    *pw = right - left;
    *ph = bottom - top;

    float invWidth = 1.0f / (float) pFrame->width;
    float invHeight = 1.0f / (float) pFrame->height;

    *pfLeft = ((float) left + 0.5f) * invWidth;
    *pfRight = ((float) right - 0.5f) * invWidth;
    *pfTop = ((float) top + 0.5f) * invHeight;
    *pfBottom = ((float) bottom - 0.5f) * invHeight;
}

void GL_Bind(int texnum)
{
    OPTICK_EVENT();

    eng()->GL_Bind.InvokeChained(texnum);
}

GLuint GL_GenTexture()
{
    OPTICK_EVENT();

    GLuint tex;
    qglGenTextures(1, &tex);
    return tex;
}

void GL_SelectTexture(GLenum target)
{
    OPTICK_EVENT();

    if (!(*p_gl_mtexable))
        return;

    qglSelectTextureSGIS(target);

    if (target != oldtarget)
    {
        cnttextures[oldtarget - TEXTURE0_SGIS] = *p_currenttexture;
        *p_currenttexture = cnttextures[target - TEXTURE0_SGIS];
        oldtarget = target;
    }
}

int GL_LoadTexture(const char* identifier, GL_TEXTURETYPE textureType, int width, int height, uint8_t* data, int mipmap, int iType, uint8_t* pPal)
{
    OPTICK_EVENT();

    return eng()->GL_LoadTexture.InvokeChained(identifier, textureType, width, height, data, mipmap, iType, pPal);
}

void GL_DisableMultitexture()
{
    OPTICK_EVENT();

    if (*p_mtexenabled)
    {
        qglDisable(GL_TEXTURE_2D);
        GL_SelectTexture(TEXTURE0_SGIS);
        *p_mtexenabled = false;
    }
}

void Draw_DecalSetName(int decal, const char* name)
{
    OPTICK_EVENT();

    eng()->Draw_DecalSetName.InvokeChained(decal, name);
}

void Draw_SpriteFrame(mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect)
{
    OPTICK_EVENT();

    Draw_Frame(pFrame, x, y, prcSubRect);
}

void Draw_SpriteFrameAdditive(mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect)
{
    OPTICK_EVENT();

    qglEnable(GL_BLEND);
    qglBlendFunc(GL_ONE, GL_ONE);
    Draw_Frame(pFrame, x, y, prcSubRect);
    qglDisable(GL_BLEND);
}

void Draw_SpriteFrameHoles(mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect)
{
    OPTICK_EVENT();

    qglEnable(GL_ALPHA_TEST);
    if (gl_spriteblend->value != 0.0)
    {
        qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        qglEnable(GL_BLEND);
    }
    Draw_Frame(pFrame, x, y, prcSubRect);
    qglDisable(GL_ALPHA_TEST);
    qglDisable(GL_BLEND);
}

void Draw_SpriteFrameGeneric(mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect, int src, int dest, int width, int height)
{
    OPTICK_EVENT();

    int old_width = pFrame->width;
    pFrame->width = width;

    int old_height = pFrame->height;
    pFrame->height = height;

    qglEnable(GL_BLEND);
    qglBlendFunc(src, dest);
    Draw_Frame(pFrame, x, y, prcSubRect);
    qglDisable(GL_BLEND);

    pFrame->width = old_width;
    pFrame->height = old_height;
}

void Draw_Frame(mspriteframe_t* pFrame, int ix, int iy, const wrect_t* prcSubRect)
{
    OPTICK_EVENT();

    float fLeft = 0.0;
    float fRight = 1.0;
    float fTop = 0.0;
    float fBottom = 1.0;

    int iWidth = pFrame->width;
    int iHeight = pFrame->height;

    float x = (float)ix + 0.5f;
    float y = (float)iy + 0.5f;

    eng()->VGUI2_ResetCurrentTexture.InvokeChained();

    if (giScissorTest)
    {
        qglScissor(scissor_x, scissor_y, scissor_width, scissor_height);
        qglEnable(GL_SCISSOR_TEST);
    }

    if (prcSubRect)
        AdjustSubRect(pFrame, &fLeft, &fRight, &fTop, &fBottom, &iWidth, &iHeight, prcSubRect);

    qglDepthMask(GL_FALSE);
    GL_Bind(pFrame->gl_texturenum);
    qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    qglBegin(GL_QUADS);
        qglTexCoord2f(fLeft, fTop);
        qglVertex2f(x, y);

        qglTexCoord2f(fRight, fTop);
        qglVertex2f((float)iWidth + x, y);

        qglTexCoord2f(fRight, fBottom);
        qglVertex2f((float)iWidth + x, (float)iHeight + y);

        qglTexCoord2f(fLeft, fBottom);
        qglVertex2f(x, (float)iHeight + y);
    qglEnd();
    qglDepthMask(GL_TRUE);
    qglDisable(GL_SCISSOR_TEST);
}

void EnableScissorTest(int x, int y, int width, int height)
{
    if (x <= p_vid->width)
    {
        scissor_x = (x < 0) ? 0 : x;
        scissor_width = p_vid->width - scissor_x;
    }

    if (y <= p_vid->height)
    {
        scissor_y = (y < 0) ? 0 : y;
        scissor_height = p_vid->height - scissor_y;
    }

    if (width <= scissor_width)
        scissor_width = (width < 0) ? 0 : width;

    if (height <= scissor_height)
        scissor_height = (height < 0) ? 0 : height;

    giScissorTest = true;
}

void DisableScissorTest()
{
    scissor_x = 0;
    scissor_y = 0;
    scissor_width = 0;
    scissor_height = 0;
    giScissorTest = false;
}
