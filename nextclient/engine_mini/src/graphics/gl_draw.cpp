#include "engine.h"

#include <SDL_video.h>
#include <glad/glad.h>
#include <optick.h>

#include "common/host.h"
#include "common/cmd.h"
#include "common/cvar.h"
#include "common/filesystem.h"
#include "common/mem.h"
#include "common/sys_dll.h"
#include "common/wad.h"
#include "common/zone.h"
#include "console/console.h"
#include "resource/texture_manager/TextureManager.h"
#include "cstrike_hack.h"

#include "view.h"
#include "gl_local.h"
#include "qgl.hpp"
#include "vgui2.h"

using namespace tex;

struct quake_mode_t
{
    const char* name;
    int minimize;
    int maximize;
};

void gl_texturemode_hook_callback(cvar_t* cvar);

cvar_t gl_ansio = {"gl_ansio", const_cast<char*>("16"), FCVAR_ARCHIVE};
cvar_t gl_palette_tex = {"gl_palette_tex", const_cast<char*>("1"), FCVAR_ARCHIVE};
cvar_t gl_texturemode = {"gl_texturemode", const_cast<char*>("GL_LINEAR_MIPMAP_LINEAR"), FCVAR_ARCHIVE};
cvar_t gl_picmip = {"gl_picmip", const_cast<char*>("0"), FCVAR_ARCHIVE};
cvar_t gl_max_size = {"gl_max_size", const_cast<char*>("1024"), FCVAR_ARCHIVE};
cvar_t gl_round_down = {"gl_round_down", const_cast<char*>("3"), FCVAR_ARCHIVE};

int gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
int gl_filter_max = GL_LINEAR;

uint8_t texgammatable[256];
int32_t lineargammatable[1024];
int32_t lightgammatable[1024];
int32_t screengammatable[1024];

GLenum oldtarget;
static int cnttextures[2] = {-1, -1}; // cached

static GLint scissor_x;
static GLint scissor_y;
static GLsizei scissor_width;
static GLsizei scissor_height;
static qboolean giScissorTest;

static GLuint translate_texture;
static uint8_t menuplyr_pixels[4096];

static PaletteHandle g_CurrentPalette;

static nitroapi::viddef_t vid;
static float v_blend[4];
static float oldgammavalue;
static float oldlightgamma;
static float oldtexgamma;
static float oldbrightness;
static uint8_t ramps[3][sizeof(texgammatable)];

static cvar_t* con_color;

static cachewad_t* menu_wad;
static cachewad_t custom_wad;
static qpic_t* draw_disc;
static char decal_names[8192];

static quake_mode_t modes[6]{
    {"GL_NEAREST", GL_NEAREST, GL_NEAREST},
    {"GL_LINEAR", GL_LINEAR, GL_LINEAR},
    {"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
    {"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
    {"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
    {"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR},
};

static cvarhook_t gl_texturemode_hook = {gl_texturemode_hook_callback, nullptr, nullptr};

static bool g_DrawInitialized;
static bool g_BuildGammaTableFirstCallPass;

void BuildGammaTable(float g)
{
    // V_Init_AfterCvarsHook must run after engine registers its cvars but before
    // the first gamma table build. BuildGammaTable is the earliest reliable hook point.
    if (!g_BuildGammaTableFirstCallPass)
    {
        g_BuildGammaTableFirstCallPass = true;
        V_Init_AfterCvarsHook();
    }

    float inverse_gamma = 0.40000001f;
    if (g != 0.0f)
    {
        inverse_gamma = 1.0f / g;
    }

    float gamma = inverse_gamma * texgamma_cvar->value;
    float brightness = brightness_cvar->value;
    float brightness_threshold = 0.125f;

    if (brightness > 0.0f)
    {
        brightness_threshold = 0.050000001f;

        if (brightness <= 1.0f)
        {
            brightness_threshold = 0.125f - brightness * brightness * 0.075f;
        }
    }

    // fill texgammatable

    for (int i = 0; i < 256; ++i)
    {
        double normalized = std::pow((double)i / 255.0, gamma) * 255.0;
        int value = std::clamp((int)normalized, 0, 255);

        texgammatable[i] = value;
    }

    // fill lightgammatable

    for (int i = 0; i < 1024; ++i)
    {
        double light = std::pow((double)i / 1023.0, lightgamma_cvar->value);
        if (brightness > 1.0f)
        {
            light *= brightness;
        }

        double mapped;
        if (brightness_threshold >= light)
        {
            mapped = light / brightness_threshold * 0.125;
        }
        else
        {
            mapped = (light - brightness_threshold) / (1.0 - brightness_threshold) * 0.875 + 0.125;
        }

        int value = (int)(std::pow(mapped, inverse_gamma) * 1023.0);
        value = std::clamp(value, 0, 1023);

        lightgammatable[i] = value;
    }

    // fill lineargammatable and screengammatable

    for (int i = 0; i < 1024; ++i)
    {
        double x = (double)i / 1023.0;
        double value = std::pow(x, gamma_cvar->value) * 1023.0;
        double inverse_table_gamma = 1.0 / gamma_cvar->value;

        lineargammatable[i] = (int)value;
        screengammatable[i] = (int)(std::pow(x, inverse_table_gamma) * 1023.0);
    }
}

void Draw_MiptexTexture(cachewad_t* wad, unsigned char* data)
{
    eng()->Draw_MiptexTexture(wad, data);
}

void FilterLightParams()
{
    if (g_bIsCStrike)
    {
        Cvar_DirectSet(lightgamma_cvar, "2.5");
        Cvar_DirectSet(texgamma_cvar, "2.0");
    }

    if (Host_GetMaxClients() > 1 && brightness_cvar->value > 2.0)
    {
        Cvar_DirectSet(brightness_cvar, "2.0");
    }

    if (gamma_cvar->value < 1.8)
    {
        Cvar_DirectSet(gamma_cvar, "1.8");
    }
    else if (gamma_cvar->value > 3.0)
    {
        Cvar_DirectSet(gamma_cvar, "3.0");
    }

    if (texgamma_cvar->value < 1.8)
    {
        Cvar_DirectSet(texgamma_cvar, "1.8");
    }
    else if (texgamma_cvar->value > 3.0)
    {
        Cvar_DirectSet(texgamma_cvar, "3.0");
    }

    if (lightgamma_cvar->value < 1.8)
    {
        Cvar_DirectSet(lightgamma_cvar, "1.8");
    }
    else if (lightgamma_cvar->value > 3.0)
    {
        Cvar_DirectSet(lightgamma_cvar, "3.0");
    }

    if (brightness_cvar->value < 0.0)
    {
        Cvar_DirectSet(brightness_cvar, "0.0");
    }
    else if (brightness_cvar->value > 100.0)
    {
        Cvar_DirectSet(brightness_cvar, "100.0");
    }
}

bool V_CheckGamma()
{
    FilterLightParams();

    if (gamma_cvar->value == oldgammavalue
        && lightgamma_cvar->value == oldlightgamma
        && texgamma_cvar->value == oldtexgamma
        && brightness_cvar->value == oldbrightness)
    {
        return false;
    }

    BuildGammaTable(gamma_cvar->value);

    oldgammavalue = gamma_cvar->value;
    oldlightgamma = lightgamma_cvar->value;
    oldtexgamma = texgamma_cvar->value;
    oldbrightness = brightness_cvar->value;

    vid.recalc_refdef = 1;

    return true;
}

void V_UpdatePalette()
{
    if (!V_CheckGamma())
    {
        return;
    }

    v_blend[0] = 0.0;
    v_blend[1] = 0.0;
    v_blend[2] = 0.0;
    v_blend[3] = 0.0;

    for (int i = 0; i < 256; ++i)
    {
        uint8_t gamma = texgammatable[i];
        ramps[0][i] = gamma;
        ramps[1][i] = gamma;
        ramps[2][i] = gamma;
    }
}

static void AdjustSubRect(
    mspriteframe_t* pFrame,
    float* pfLeft,
    float* pfRight,
    float* pfTop,
    float* pfBottom,
    int* pw,
    int* ph,
    const wrect_t* prcSubRect
)
{
    OPTICK_EVENT();

    if (!prcSubRect)
        return;

    int left = std::max(0, prcSubRect->left);
    int right = std::min(*pw, prcSubRect->right);
    int top = std::max(0, prcSubRect->top);
    int bottom = std::min(*ph, prcSubRect->bottom);

    if (left >= right || top >= bottom)
    {
        return;
    }

    *pw = right - left;
    *ph = bottom - top;

    float inv_width = 1.0f / (float)pFrame->width;
    float inv_height = 1.0f / (float)pFrame->height;

    *pfLeft = ((float)left + 0.5f) * inv_width;
    *pfRight = ((float)right - 0.5f) * inv_width;
    *pfTop = ((float)top + 0.5f) * inv_height;
    *pfBottom = ((float)bottom - 0.5f) * inv_height;
}

void GL_Bind(int texnum)
{
    OPTICK_EVENT();

    if (*p_currenttexture == texnum)
    {
        return;
    }

    *p_currenttexture = texnum;
    qglBindTexture(GL_TEXTURE_2D, texnum);

    // This approach with shared palettes doesn't seem very effective; while it saves a little memory, it causes a stall.
    // Given that this extension only works on older hardware, the situation worsens dramatically.
    // Perhaps it would be best to remove support for hardware palettes entirely.

    if (!qglColorTableEXT)
    {
        return;
    }

    Texture* texture = g_TextureManagerGlob.GetByGLId(texnum);
    if (texture == nullptr)
    {
        return;
    }

    PaletteHandle palette_handle = texture->palette;

    Palette* palette = g_PaletteManagerGlob.Get(palette_handle);
    if (palette != nullptr && g_CurrentPalette != palette_handle)
    {
        g_CurrentPalette = palette_handle;

        qglColorTableEXT(GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, 256, GL_RGB, GL_UNSIGNED_BYTE, palette->colors);
    }
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

    if (!gl_mtexable)
    {
        return;
    }

    qglSelectTextureSGIS(target);

    if (target == oldtarget)
    {
        return;
    }

    cnttextures[oldtarget - TEXTURE0_SGIS] = *p_currenttexture;
    *p_currenttexture = cnttextures[target - TEXTURE0_SGIS];
    oldtarget = target;
}

void GL_UnloadTextures()
{
    OPTICK_EVENT();

    g_TextureManagerGlob.ClearSession();
}

void GL_UnloadTexture(const char* identifier)
{
    OPTICK_EVENT();

    char identifier_new[TexIdentifierStr::kMaxSize + 1];
    V_sprintf_safe(identifier_new, "%s-", identifier);

    g_TextureManagerGlob.ForEach([&identifier_new](Texture* texture, TextureHandle handle, TextureLifetime lifetime) {
        if (std::string_view(texture->identifier).starts_with(identifier_new))
        {
            g_TextureManagerGlob.Destroy(handle);
            return false;
        }

        return true;
    });
}

int GL_LoadTexture2(
    const char* identifier,
    GL_TEXTURETYPE textureType,
    int width,
    int height,
    uint8_t* data,
    qboolean mipmap,
    int iType,
    uint8_t* pPal,
    int filter
)
{
    OPTICK_EVENT();

    TextureLifetime lifetime = TextureLifetime::Persistent;

    if (*p_host_initialized && (!cls->demoplayback || cl->maxclients))
    {
        switch (textureType)
        {
            case GLT_HUDSPRITE:
            case GLT_STUDIO:
            case GLT_WORLD:
            case GLT_SPRITE:
                lifetime = TextureLifetime::Session;
                break;

            default:
                break;
        }
    }

    char identifier_new[TexIdentifierStr::kMaxSize + 1];
    V_sprintf_safe(identifier_new, "%s-%dx%d", identifier, width, height);

    TextureHandle texture_handle =
        g_TextureManagerGlob.Load(identifier_new, lifetime, (TextureFormat)iType, width, height, data, mipmap, pPal, filter);

    if (!texture_handle.IsValid())
    {
        return -1;
    }

    return g_TextureManagerGlob.Get(texture_handle)->texnum;
}

int GL_LoadTexture(
    const char* identifier,
    GL_TEXTURETYPE textureType,
    int width,
    int height,
    uint8_t* data,
    int mipmap,
    int iType,
    uint8_t* pPal
)
{
    OPTICK_EVENT();

    return GL_LoadTexture2(identifier, textureType, width, height, data, mipmap, iType, pPal, gl_filter_max);
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

static void DrawQuad(float x_left, float y_top, float x_right, float y_bottom, float uv_left, float uv_top, float uv_right, float uv_bottom)
{
    OPTICK_EVENT();

    qglBegin(GL_QUADS);

    qglTexCoord2f(uv_left, uv_top);
    qglVertex2f(x_left, y_top);

    qglTexCoord2f(uv_right, uv_top);
    qglVertex2f(x_right, y_top);

    qglTexCoord2f(uv_right, uv_bottom);
    qglVertex2f(x_right, y_bottom);

    qglTexCoord2f(uv_left, uv_bottom);
    qglVertex2f(x_left, y_bottom);

    qglEnd();
}

static void SetAlphaColor(colorVec* color, float alpha)
{
    OPTICK_EVENT();

    float final_alpha = alpha / 255.0f;

    if (color)
    {
        float r = (color->r * final_alpha) / 255.0f;
        float g = (color->g * final_alpha) / 255.0f;
        float b = (color->b * final_alpha) / 255.0f;
        qglColor3f(r, g, b);
    }
    else
    {
        qglColor3f(final_alpha, final_alpha, final_alpha);
    }
}

static void FillTranslatedPixels(unsigned int* trans, int width, int height)
{
    OPTICK_EVENT();

    const int tile_size = 64;
    const unsigned int transparent_color = 0xFF0000FF;
    const unsigned int opaque_color = 0xFFFFFFFF;

    for (int tile_y = 0; tile_y < tile_size; ++tile_y)
    {
        for (int tile_x = 0; tile_x < tile_size; ++tile_x)
        {
            int src_x = (tile_x * width) / tile_size;
            int src_y = (tile_y * height) / tile_size;

            bool is_transparent = menuplyr_pixels[src_y * width + src_x] != 0xFF;
            trans[tile_y * tile_size + tile_x] = is_transparent ? transparent_color : opaque_color;
        }
    }
}

void Draw_TransPicTranslate(int x, int y, qpic_t* pic)
{
    OPTICK_EVENT();

    if (!pic)
    {
        return;
    }

    VGUI2_ResetCurrentTexture();

    if (!translate_texture)
    {
        qglGenTextures(1, &translate_texture);
    }

    GL_Bind(translate_texture);

    unsigned int trans[4096];
    FillTranslatedPixels(trans, pic->width, pic->height);

    qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value);

    float x_right = x + pic->width;
    float y_bottom = y + pic->height;

    GLfloat* tex_coords = (GLfloat*)((char*)pic + sizeof(qpic_t));
    GLfloat uv_left = tex_coords[0];
    GLfloat uv_top = tex_coords[1];
    GLfloat uv_right = tex_coords[2];
    GLfloat uv_bottom = tex_coords[3];

    DrawQuad(x, y, x_right, y_bottom, uv_left, uv_top, uv_right, uv_bottom);
}

void Draw_AlphaPic(int x, int y, qpic_t* pic, colorVec* pc, int alpha)
{
    OPTICK_EVENT();

    if (!pic)
    {
        return;
    }

    VGUI2_ResetCurrentTexture();

    qglEnable(GL_ALPHA_TEST);
    qglEnable(GL_BLEND);
    qglBlendFunc(GL_ONE, GL_ONE);
    qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    SetAlphaColor(pc, (float)alpha);

    int texnum = *(int*)pic->data;
    GL_Bind(texnum);

    float x_right = x + pic->width;
    float y_bottom = y + pic->height;

    GLfloat* tex_coords = (GLfloat*)((char*)pic + sizeof(qpic_t));
    GLfloat uv_left = tex_coords[0];
    GLfloat uv_top = tex_coords[1];
    GLfloat uv_right = tex_coords[2];
    GLfloat uv_bottom = tex_coords[3];

    DrawQuad(x, y, x_right, y_bottom, uv_left, uv_top, uv_right, uv_bottom);

    qglDisable(GL_BLEND);
}

void Draw_AlphaAddPic(int x, int y, qpic_t* pic, colorVec* color, int alpha)
{
    OPTICK_EVENT();

    if (!pic)
    {
        return;
    }

    VGUI2_ResetCurrentTexture();

    qglEnable(GL_TEXTURE_2D);
    qglEnable(GL_BLEND);
    qglBlendFunc(GL_SRC_ALPHA, GL_ONE);
    qglEnable(GL_ALPHA_TEST);
    qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    if (color)
    {
        qglColor4f(color->r / 255.0f, color->g / 255.0f, color->b / 255.0f, alpha / 255.0f);
    }
    else
    {
        qglColor4f(1.0f, 1.0f, 1.0f, alpha / 255.0f);
    }

    int texnum = *(int*)pic->data;
    GL_Bind(texnum);

    float x_right = x + pic->width;
    float y_bottom = y + pic->height;

    GLfloat* tex_coords = (GLfloat*)((char*)pic + sizeof(qpic_t));
    GLfloat uv_left = tex_coords[0];
    GLfloat uv_top = tex_coords[1];
    GLfloat uv_right = tex_coords[2];
    GLfloat uv_bottom = tex_coords[3];

    DrawQuad(x, y, x_right, y_bottom, uv_left, uv_top, uv_right, uv_bottom);

    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
}

void Draw_AlphaSubPic(int x_dest, int y_dest, int x_src, int y_src, int width, int height, qpic_t* pic, colorVec* color, int alpha)
{
    OPTICK_EVENT();

    const GLfloat inv_255 = 1.0f / 255.0f;

    if (!pic)
    {
        return;
    }

    VGUI2_ResetCurrentTexture();

    qglEnable(GL_BLEND);
    qglEnable(GL_ALPHA_TEST);
    qglBlendFunc(GL_ONE, GL_ONE);
    qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    float alpha_factor = (float)alpha / 255.0f;
    float r = color->r * inv_255 * alpha_factor;
    float g = color->g * inv_255 * alpha_factor;
    float b = color->b * inv_255 * alpha_factor;
    qglColor4f(r, g, b, 1.0f);

    int texnum = *(int*)pic->data;
    GL_Bind(texnum);

    float uv_left = (float)x_src / pic->width;
    float uv_top = (float)y_src / pic->height;
    float uv_right = uv_left + (float)width / pic->width;
    float uv_bottom = uv_top + (float)height / pic->height;

    float x_left = (float)x_dest;
    float y_top = (float)y_dest;
    float x_right = x_left + width;
    float y_bottom = y_top + height;

    DrawQuad(x_left, y_top, x_right, y_bottom, uv_left, uv_top, uv_right, uv_bottom);
}

void Draw_Pic2(int x, int y, int w, int h, qpic_t* pic)
{
    OPTICK_EVENT();

    if (!pic)
    {
        return;
    }

    VGUI2_ResetCurrentTexture();

    qglEnable(GL_TEXTURE_2D);
    qglDisable(GL_BLEND);
    qglEnable(GL_ALPHA_TEST);
    qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)gl_filter_max);

    int texnum = *(int*)pic->data;
    GL_Bind(texnum);

    float x_right = (float)w + pic->width;
    float y_bottom = (float)h + pic->height;

    GLfloat* tex_coords = (GLfloat*)((char*)pic + sizeof(qpic_t));
    GLfloat uv_left = tex_coords[0];
    GLfloat uv_top = tex_coords[1];
    GLfloat uv_right = tex_coords[2];
    GLfloat uv_bottom = tex_coords[3];

    DrawQuad(x, y, x_right, y_bottom, uv_left, uv_top, uv_right, uv_bottom);
}

void Draw_Pic(int x, int y, qpic_t* pic)
{
    OPTICK_EVENT();

    if (!pic)
    {
        return;
    }

    VGUI2_ResetCurrentTexture();

    qglEnable(GL_TEXTURE_2D);
    qglDisable(GL_BLEND);
    qglDisable(GL_DEPTH_TEST);
    qglEnable(GL_ALPHA_TEST);
    qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    int texnum = *(int*)pic->data;
    GL_Bind(texnum);

    float x_right = (float)x + pic->width;
    float y_bottom = (float)y + pic->height;

    GLfloat* tex_coords = (GLfloat*)((char*)pic + sizeof(qpic_t));
    GLfloat uv_left = tex_coords[0];
    GLfloat uv_top = tex_coords[1];
    GLfloat uv_right = tex_coords[2];
    GLfloat uv_bottom = tex_coords[3];

    DrawQuad(x, y, x_right, y_bottom, uv_left, uv_top, uv_right, uv_bottom);
}

void Draw_DecalSetName(int decal, const char* name)
{
    OPTICK_EVENT();

    eng()->Draw_DecalSetName(decal, name);
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

void Draw_SpriteFrameGeneric(
    mspriteframe_t* pFrame,
    unsigned short* pPalette,
    int x,
    int y,
    const wrect_t* prcSubRect,
    int src,
    int dest,
    int width,
    int height
)
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

    float left = 0.0;
    float right = 1.0;
    float top = 0.0;
    float bottom = 1.0;

    int width = pFrame->width;
    int height = pFrame->height;

    float x = (float)ix + 0.5f;
    float y = (float)iy + 0.5f;

    VGUI2_ResetCurrentTexture();

    if (giScissorTest)
    {
        qglScissor(scissor_x, scissor_y, scissor_width, scissor_height);
        qglEnable(GL_SCISSOR_TEST);
    }

    if (prcSubRect)
    {
        AdjustSubRect(pFrame, &left, &right, &top, &bottom, &width, &height, prcSubRect);
    }

    qglDepthMask(GL_FALSE);
    GL_Bind(pFrame->gl_texturenum);
    qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    qglBegin(GL_QUADS);

    qglTexCoord2f(left, top);
    qglVertex2f(x, y);

    qglTexCoord2f(right, top);
    qglVertex2f((float)width + x, y);

    qglTexCoord2f(right, bottom);
    qglVertex2f((float)width + x, (float)height + y);

    qglTexCoord2f(left, bottom);
    qglVertex2f(x, (float)height + y);

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
    {
        scissor_width = (width < 0) ? 0 : width;
    }

    if (height <= scissor_height)
    {
        scissor_height = (height < 0) ? 0 : height;
    }

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

qpic_t* LoadTransPic(const char* pszName, qpic_t* ppic)
{
    OPTICK_EVENT();

    if (!ppic)
    {
        return ppic;
    }

    int width = ppic->width;
    int height = ppic->height;
    uint8_t* palette = ppic->data + width * height + 2;

    char identifier_new[TexIdentifierStr::kMaxSize + 1];
    V_sprintf_safe(identifier_new, "%s-%dx%d", pszName, width, height);

    TextureHandle texture_handle = g_TextureManagerGlob.Load(
        identifier_new, TextureLifetime::Persistent, TextureFormat::Alpha, width, height, ppic->data, false, palette, gl_filter_max
    );

    if (!texture_handle.IsValid())
    {
        return nullptr;
    }

    Texture* texture = g_TextureManagerGlob.Get(texture_handle);

    glpic_t* pic_new = (glpic_t*)Mem_Malloc(sizeof(glpic_t));
    pic_new->pic.width = width;
    pic_new->pic.height = height;
    pic_new->texnum = texture->texnum;
    pic_new->sl = 0;
    pic_new->sh = 1;
    pic_new->tl = 0;
    pic_new->th = 1;

    return (qpic_t*)pic_new;
}

void Draw_CacheWadInitFromFile(FileHandle_t hFile, int len, const char* name, int cacheMax, cachewad_t* wad)
{
    OPTICK_EVENT();

    wadinfo_t header;

    FS_Read(&header, sizeof(wadinfo_t), hFile);
    if (*(int*)header.identification != MAKEID('W', 'A', 'D', '3'))
    {
        Sys_Error("Wad file %s doesn't have WAD3 id\n", name);
    }

    wad->lumps = (lumpinfo_s*)Mem_Malloc(len - header.infotableofs);
    FS_Seek(hFile, header.infotableofs, FILESYSTEM_SEEK_HEAD);
    FS_Read(wad->lumps, len - header.infotableofs, hFile);

    for (int i = 0; i < header.numlumps; ++i)
    {
        W_CleanupName(wad->lumps[i].name, wad->lumps[i].name);
    }

    wad->lumpCount = header.numlumps;
    wad->cacheMax = cacheMax;
    wad->cacheCount = 0;
    wad->name = Mem_Strdup(name);

    wad->cache = (cacheentry_t*)Mem_Malloc(sizeof(cacheentry_t) * cacheMax);
    Q_memset(wad->cache, 0, sizeof(cacheentry_t) * cacheMax);

    wad->tempWad = 0;
    wad->pfnCacheBuild = 0;
    wad->cacheExtra = 0;
}

void Draw_CacheWadInit(const char* name, int cacheMax, cachewad_t* wad)
{
    OPTICK_EVENT();

    FileHandle_t file = FS_Open(name, "rb");
    if (!file)
    {
        Sys_Error("Draw_LoadWad: Couldn't open %s\n", name);
    }

    int size = FS_Size(file);
    Draw_CacheWadInitFromFile(file, size, name, cacheMax, wad);
    FS_Close(file);
}

void Draw_FreeWad(cachewad_t* pWad)
{
    OPTICK_EVENT();

    if (!pWad)
    {
        return;
    }

    if (pWad->lumps)
    {
        Mem_Free(pWad->lumps);
        pWad->lumps = nullptr;
    }

    if (pWad->name)
    {
        Mem_Free(pWad->name);
        pWad->name = nullptr;
    }

    if (pWad->basedirs)
    {
        for (int i = 0; i < pWad->numpaths; ++i)
        {
            if (pWad->basedirs[i])
            {
                Mem_Free(pWad->basedirs[i]);
                pWad->basedirs[i] = nullptr;
            }
        }

        Mem_Free(pWad->basedirs);
        pWad->basedirs = nullptr;
    }

    if (pWad->lumppathindices)
    {
        Mem_Free(pWad->lumppathindices);
        pWad->lumppathindices = nullptr;
    }

    if (pWad->cache)
    {
        if (!pWad->tempWad)
        {
            for (int i = 0; i < pWad->cacheCount; ++i)
            {
                if (Cache_Check(&pWad->cache[i].cache))
                {
                    Cache_Free(&pWad->cache[i].cache);
                }
            }
        }

        Mem_Free(pWad->cache);
        pWad->cache = nullptr;
    }
}

void Draw_CacheWadHandler(cachewad_t* wad, PFNCACHE fn, int extraDataSize)
{
    wad->pfnCacheBuild = fn;
    wad->cacheExtra = extraDataSize;
}

void Draw_ResetTextColor()
{
    int r, g, b;

    if (sscanf(con_color->string, "%i %i %i", &r, &g, &b) == 3)
    {
        VGUI2_Draw_SetTextColor(r, g, b);
    }
}

void GL_Texels_f()
{
    Con_Printf("Obsolete\n");
}

qpic_t* LoadTransBMP(const char* name)
{
    OPTICK_EVENT();

    qpic_t* lump_name = (qpic_t*)W_GetLumpName(0, name);
    return LoadTransPic(name, lump_name);
}

int GL_LoadPicTexture(qpic_t* pic, const char* pszName)
{
    OPTICK_EVENT();

    return GL_LoadTexture2(
        pszName,
        GLT_SYSTEM,
        pic->width,
        pic->height,
        pic->data,
        false,
        (int)TextureFormat::Alpha,
        pic->data + pic->height * pic->width + 2,
        gl_filter_max
    );
}

qpic_t* Draw_PicFromWad(const char* name)
{
    OPTICK_EVENT();

    qpic_t* p;
    glpic_t* gl;

    p = (qpic_t*)W_GetLumpName(0, name);
    gl = (glpic_t*)p->data;

    gl->texnum = GL_LoadPicTexture(p, name);
    gl->sl = 0;
    gl->sh = 1;
    gl->tl = 0;
    gl->th = 1;

    return p;
}

void gl_texturemode_hook_callback(cvar_t* cvar)
{
    OPTICK_EVENT();

    if (!cvar)
    {
        return;
    }

    int mode_index = 0;
    for (int i = 0; i < 6; ++i)
    {
        if (!Q_strcasecmp(modes[i].name, cvar->string))
        {
            mode_index = i;
            break;
        }

        if (i == 5)
        {
            Con_Printf("gl_texturemode_hook_callback: bad filter name\n");
            return;
        }
    }

    gl_filter_min = modes[mode_index].minimize;
    gl_filter_max = modes[mode_index].maximize;

    g_TextureManagerGlob.ForEach([](Texture* texture, TextureHandle handle, TextureLifetime lifetime) {
        if (texture->mipmap)
        {
            GL_Bind(texture->texnum);
            qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
            qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
            qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value);
        }
    });
}

void Draw_Init()
{
    g_DrawInitialized = true;

    VGUI2_Draw_Init();

    Cmd_AddCommand("gl_texels", GL_Texels_f);
    Cvar_RegisterVariable(&gl_max_size);
    Cvar_RegisterVariable(&gl_round_down);
    Cvar_RegisterVariable(&gl_picmip);
    Cvar_RegisterVariable(&gl_palette_tex);
    Cvar_RegisterVariable(&gl_texturemode);
    Cvar_RegisterVariable(&gl_ansio);
    Cvar_HookVariable(gl_texturemode.name, &gl_texturemode_hook);

    if (registry->ReadInt("vid_level", 0) > 0)
    {
        qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value);
    }

    menu_wad = (cachewad_t*)Mem_ZeroMalloc(sizeof(cachewad_t));
    Draw_CacheWadInit("cached.wad", 16, menu_wad);
    menu_wad->tempWad = 1;

    Draw_CacheWadHandler(&custom_wad, Draw_MiptexTexture, 32);

    if (qglColorTableEXT && gl_palette_tex.value != 0.0)
    {
        qglEnable(GL_SHARED_TEXTURE_PALETTE_EXT);
    }

    Q_memset(&decal_names, 0, sizeof(decal_names));

    for (int i = 0; i < 256; ++i)
    {
        texgammatable[i] = i;
    }

    draw_disc = LoadTransBMP("lambda");

    con_color = Cvar_FindVar("con_color");
    Draw_ResetTextColor();
}

void Draw_Shutdown()
{
    if (!g_DrawInitialized)
    {
        return;
    }

    g_DrawInitialized = false;

    Draw_FreeWad(menu_wad);

    if (menu_wad)
    {
        Mem_Free(menu_wad);
    }

    menu_wad = nullptr;
}
