#include "engine.h"

#include <SDL_video.h>
#include <Windows.h>
#include <common/cvar.h>
#include <glad/glad.h>

#include "common/cmd.h"
#include "common/sys_dll.h"
#include "console/console.h"
#include "graphics/detailtexture.h"
#include "graphics/gl_local.h"
#include "graphics/gl_draw.h"

static bool g_DisableMultitexture;

static const char* gl_extensions;
static const char* gl_vendor;
static const char* gl_renderer;
static const char* gl_version;

// Cvars mirrored from the engine's R_Init
static cvar_t gl_dither_cvar_tmp{"gl_dither", const_cast<char*>("1"), FCVAR_ARCHIVE, 1.f};
static cvar_t gl_spriteblend_cvar_tmp{"gl_spriteblend", const_cast<char*>("1"), FCVAR_ARCHIVE, 1.f};
cvar_t* r_lightmap;
cvar_t* gl_clear;
cvar_t* r_novis;
cvar_t* r_fullbright;
cvar_t* gl_monolights;
cvar_t* gl_wireframe;
cvar_t* r_dynamic;
cvar_t* gl_alphamin;
cvar_t* gl_polyoffset;

// These globals are mirrored by the original engine's hw.dll; the engine's
// rendering code reads its own copies, so both must be kept in sync.
int TEXTURE0_SGIS = QGL_TEXTURE0_SGIS;
int TEXTURE1_SGIS = QGL_TEXTURE1_SGIS;
int TEXTURE2_SGIS = QGL_TEXTURE2_SGIS;
qboolean bSupportsNPOTTextures;
int gl_mtexable;

// and these:
//   qglColorTableEXT     - used in R_LoadSkys
//   qglBindTexture       - used in multiple places
//   qglMTexCoord2fSGIS   - used in multiple places
//   qglSelectTextureSGIS - used in multiple places

static void CheckTextureExtensions()
{
    const char* extension;

    if (!gl_extensions)
    {
        qglColorTableEXT = nullptr;
        extension = nullptr;
    }
    else if (!Q_strstr(gl_extensions, "GL_EXT_paletted_texture") || !Q_strstr(gl_extensions, "GL_EXT_shared_texture_palette"))
    {
        qglColorTableEXT = nullptr;
        extension = gl_extensions;
    }
    else
    {
        qglColorTableEXT = (decltype(qglColorTableEXT))SDL_GL_GetProcAddress("glColorTableEXT");
        extension = gl_extensions;
        Con_DPrintf(ConLogType::Info, "Found paletted texture extension.\n");
    }

    if (extension && Q_strstr(extension, "GL_EXT_texture_object"))
    {
        qglBindTexture = (decltype(qglBindTexture))SDL_GL_GetProcAddress("glBindTextureEXT");
        if (!qglBindTexture)
        {
            Sys_Error("GetProcAddress for BindTextureEXT failed");
        }
    }
}

static void CheckMultiTextureExtensions()
{
    if (g_DisableMultitexture)
    {
        Con_DPrintf(ConLogType::Info, "Multitexture is disabled.\n");
        return;
    }

    if (gl_extensions && V_strstr(gl_extensions, "GL_ARB_multitexture "))
    {
        Con_DPrintf(ConLogType::Info, "ARB Multitexture extensions found.\n");

        qglMTexCoord2fSGIS = (decltype(qglMTexCoord2fSGIS))SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
        qglSelectTextureSGIS = (decltype(qglSelectTextureSGIS))SDL_GL_GetProcAddress("glActiveTextureARB");

        TEXTURE0_SGIS = GL_TEXTURE0_ARB;
        TEXTURE1_SGIS = GL_TEXTURE1_ARB;
        TEXTURE2_SGIS = GL_TEXTURE2_ARB;

        oldtarget = TEXTURE0_SGIS;

        gl_mtexable = 2;
        GL_SelectTexture(TEXTURE0_SGIS);

        if (V_strstr(gl_extensions, "GL_ARB_texture_env_combine ") || V_strstr(gl_extensions, "GL_EXT_texture_env_combine "))
        {
            GLint num;
            qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &num);

            if (num > 2)
            {
                Con_DPrintf(ConLogType::Info, "%d texture units.  Detail texture supported.\n", num);
                gl_mtexable = num;
                DT_Initialize();
            }
        }
    }
    else if (gl_extensions && V_strstr(gl_extensions, "GL_SGIS_multitexture "))
    {
        Con_DPrintf(ConLogType::Info, "Multitexture extensions found.\n");

        qglMTexCoord2fSGIS = (decltype(qglMTexCoord2fSGIS))SDL_GL_GetProcAddress("glMTexCoord2fSGIS");
        qglSelectTextureSGIS = (decltype(qglSelectTextureSGIS))SDL_GL_GetProcAddress("glSelectTextureSGIS");

        TEXTURE0_SGIS = QGL_TEXTURE0_SGIS;
        TEXTURE1_SGIS = QGL_TEXTURE1_SGIS;
        TEXTURE2_SGIS = QGL_TEXTURE2_SGIS;

        oldtarget = TEXTURE0_SGIS;
        gl_mtexable = 2;
        GL_SelectTexture(TEXTURE0_SGIS);
    }
    else
    {
        Con_DPrintf(ConLogType::Info, "NO Multitexture extensions found.\n");
    }
}


void GL_SetMode_Subscriber(void* mainwindow, HDC* pmaindc, HGLRC* pbaseRC, const char* pszDriver, const char* pszCmdLine, bool result)
{
    if (!result)
        return;

    QGL_Init();

    gl_extensions = (const char*)qglGetString(GL_EXTENSIONS);
}

void GL_Config()
{
    // Host_Init() call order:
    //   GL_Init -> VID_Init -> Draw_Init -> ... -> R_Init
    // Draw_Init sub-calls reference gl_dither and gl_spriteblend, but the engine
    // only registers them later in R_Init. Pre-assign temporary cvar_t objects
    // here so the pointers are valid when Draw_Init runs.
    gl_dither_cvar = &gl_dither_cvar_tmp;
    gl_spriteblend = &gl_spriteblend_cvar_tmp;

    // Others from R_Init
    r_lightmap = Cvar_FindVar("r_lightmap");
    gl_clear = Cvar_FindVar("gl_clear");
    r_novis = Cvar_FindVar("r_novis");
    r_fullbright = Cvar_FindVar("r_fullbright");
    gl_monolights = Cvar_FindVar("gl_monolights");
    gl_wireframe = Cvar_FindVar("gl_wireframe");
    r_dynamic = Cvar_FindVar("r_dynamic");
    gl_alphamin = Cvar_FindVar("gl_alphamin");
    gl_polyoffset = Cvar_FindVar("gl_polyoffset");

    // Original engine code below
    
    Cbuf_InsertText("exec hw/opengl.cfg\n");

    char gl_renderer_lower[256];
    V_strcpy_safe(gl_renderer_lower, gl_renderer);
    V_strlower(gl_renderer_lower);

    if (V_strstr(gl_vendor, "3Dfx"))
    {
        if (V_strstr(gl_renderer, "Voodoo(tm)"))
        {
            Cbuf_InsertText("exec hw/3DfxVoodoo1.cfg\n");
        }
        else if (V_strstr(gl_renderer, "Voodoo^2"))
        {
            Cbuf_InsertText("exec hw/3DfxVoodoo2.cfg\n");
        }
        else
        {
            Cbuf_InsertText("exec hw/3Dfx.cfg\n");
        }
    }
    else if (V_strstr(gl_vendor, "NVIDIA"))
    {
        if (V_strstr(gl_renderer, "RIVA 128"))
        {
            Cbuf_InsertText("exec hw/riva128.cfg\n");
        }
        else if (V_strstr(gl_renderer, "TNT"))
        {
            Cbuf_InsertText("exec hw/rivaTNT.cfg\n");
        }
        else if (V_strstr(gl_renderer, "Quadro") || V_strstr(gl_renderer, "GeForce"))
        {
            Cbuf_InsertText("exec hw/geforce.cfg\n");
        }
    }
    else if (V_strstr(gl_renderer_lower, "riva tnt") || V_strstr(gl_renderer_lower, "velocity 4400"))
    {
        Cbuf_InsertText("exec hw/rivaTNT.cfg\n");
    }
    else if (V_strstr(gl_vendor, "PCX2"))
    {
        Cbuf_InsertText("exec hw/PowerVRPCX2.cfg\n");
    }
    else if (V_strstr(gl_vendor, "PowerVR"))
    {
        Cbuf_InsertText("exec hw/PowerVRSG.cfg\n");
    }
    else if (V_strstr(gl_vendor, "V2200"))
    {
        Cbuf_InsertText("exec hw/V2200.cfg\n");
    }
    else if (V_strstr(gl_vendor, "3Dlabs"))
    {
        Cbuf_InsertText("exec hw/3Dlabs.cfg\n");
    }
    else if (V_strstr(gl_vendor, "Matrox"))
    {
        Cbuf_InsertText("exec hw/matrox.cfg\n");
    }
    else if (V_strstr(gl_vendor, "ATI") && (V_strstr(gl_renderer, "Rage") || V_strstr(gl_renderer, "RAGE")) && V_strstr(gl_renderer, "128"))
    {
        Cbuf_InsertText("exec hw/ATIRage128.cfg\n");
    }
    else
    {
        if (V_strstr(gl_renderer, "Matrox") && V_strstr(gl_renderer, "G200"))
        {
            Cbuf_InsertText("exec hw/G200d3d.cfg\n");
        }
        else if (V_strstr(gl_renderer, "ATI") && (V_strstr(gl_renderer, "Rage") || V_strstr(gl_renderer, "RAGE")) &&
                 V_strstr(gl_renderer, "128"))
        {
            Cbuf_InsertText("exec hw/ATIRage128d3d.cfg\n");
        }
        else if (V_strstr(gl_vendor, "NVIDIA"))
        {
            Cbuf_InsertText("exec hw/nvidiad3d.cfg\n");
        }
    }
}

// The original engine's GL_Init runs before this one. Both execute
// CheckMultiTextureExtensions(), which searches gl_extensions for
// "GL_ARB_multitexture ". Patching data_arb_multitexture with a fake string
// is the way to prevent the engine's own copy from enabling multitexture.
void GL_Init()
{
    g_DisableMultitexture = g_UserConfig->get_value_int("disable_multitexture", 0);

    if (g_DisableMultitexture)
    {
        V_strcpy_safe(*eng()->data_arb_multitexture, "fake_extension 0000 ");
    }

    gl_vendor = (const char*)qglGetString(GL_VENDOR);
    if (!gl_vendor)
    {
        Sys_Error("Failed to query gl vendor string");
    }
    Con_DPrintf(ConLogType::Info, "GL_VENDOR: %s\n", gl_vendor);

    gl_renderer = (const char*)qglGetString(GL_RENDERER);
    Con_DPrintf(ConLogType::Info, "GL_RENDERER: %s\n", gl_renderer);

    gl_version = (const char*)qglGetString(GL_VERSION);
    Con_DPrintf(ConLogType::Info, "GL_VERSION: %s\n", gl_version);

    gl_extensions = (const char*)qglGetString(GL_EXTENSIONS);
    bSupportsNPOTTextures = FALSE;

    if (COM_CheckParm("-glext"))
    {
        Con_DPrintf(ConLogType::Info, "GL_EXTENSIONS: %s\n", gl_extensions);
    }

    int r, g, b, a;
    int depth;
    if (SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r))
    {
        r = 0;
        Con_DPrintf(ConLogType::Info, "Failed to get GL RED size (%s)\n", SDL_GetError());
    }

    if (SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g))
    {
        g = 0;
        Con_DPrintf(ConLogType::Info, "Failed to get GL GREEN size (%s)\n", SDL_GetError());
    }

    if (SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b))
    {
        b = 0;
        Con_DPrintf(ConLogType::Info, "Failed to get GL BLUE size (%s)\n", SDL_GetError());
    }

    if (SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &a))
    {
        a = 0;
        Con_DPrintf(ConLogType::Info, "Failed to get GL ALPHA size (%s)\n", SDL_GetError());
    }

    if (SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depth))
    {
        depth = 0;
        Con_DPrintf(ConLogType::Info, "Failed to get GL DEPTH size (%s)\n", SDL_GetError());
    }

    Con_DPrintf(ConLogType::Info, "GL_SIZES:  r:%d g:%d b:%d a:%d depth:%d\n", r, g, b, a, depth);
    CheckTextureExtensions();
    CheckMultiTextureExtensions();

    if (gl_extensions && V_strstr(gl_extensions, "GL_ARB_texture_non_power_of_two"))
    {
        bSupportsNPOTTextures = TRUE;
    }

    qglClearColor(1.0, 0.0, 0.0, 0.0);
    qglCullFace(GL_FRONT);
    qglEnable(GL_TEXTURE_2D);
    qglEnable(GL_ALPHA_TEST);
    qglAlphaFunc(GL_NOTEQUAL, 0.0);
    qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    qglShadeModel(GL_FLAT);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value);
    qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    GL_Config();
}
