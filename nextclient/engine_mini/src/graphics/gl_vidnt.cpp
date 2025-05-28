#include "../engine.h"
#include <Windows.h>

#include "gl_local.h"
#include "../console/console.h"
#include "../common/sys_dll.h"

const char *gl_extensions;

int TEXTURE0_SGIS;
int TEXTURE1_SGIS;
int TEXTURE2_SGIS;

bool gDisableMultitexture = false;

void CheckTextureExtensions()
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
        qglColorTableEXT = (decltype(qglColorTableEXT))sdl2()->SDL_GL_GetProcAddress("glColorTableEXT");
        extension = gl_extensions;
        Con_DPrintf(ConLogType::Info, "Found paletted texture extension.\n");
    }

    if (extension && Q_strstr(extension, "GL_EXT_texture_object"))
    {
        qglBindTexture = (decltype(qglBindTexture))sdl2()->SDL_GL_GetProcAddress("glBindTextureEXT");
        if (!qglBindTexture)
            Sys_Error("GetProcAddress for BindTextureEXT failed");
    }
}

void CheckMultiTextureExtensions()
{
    if (gDisableMultitexture)
    {
        Con_DPrintf(ConLogType::Info, "Multitexture is disabled.\n");
        return;
    }

    if (gl_extensions && Q_strstr(gl_extensions, "GL_ARB_multitexture "))
    {
        Con_DPrintf(ConLogType::Info, "ARB Multitexture extensions found.\n");

        qglMTexCoord2fSGIS = (decltype(qglMTexCoord2fSGIS))sdl2()->SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
        qglSelectTextureSGIS = (decltype(qglSelectTextureSGIS))sdl2()->SDL_GL_GetProcAddress("glActiveTextureARB");

        TEXTURE0_SGIS = GL_TEXTURE0_ARB;
        TEXTURE1_SGIS = GL_TEXTURE1_ARB;
        TEXTURE2_SGIS = GL_TEXTURE2_ARB;

        oldtarget = TEXTURE0_SGIS;
        *p_gl_mtexable = 2;
        GL_SelectTexture(TEXTURE0_SGIS);

        if (Q_strstr(gl_extensions, "GL_ARB_texture_env_combine ") ||
            Q_strstr(gl_extensions, "GL_EXT_texture_env_combine "))
        {
            GLint num;
            qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &num);

            if (num > 2)
            {
                Con_DPrintf(ConLogType::Info, "%d texture units.  Detail texture supported.\n", num);
                *p_gl_mtexable = num;
                // TODO at this moment DT_Initialize already initialized, need to be uncomment when we stop using GL_Init_Subscriber
                //eng()->DT_Initialize.InvokeChained();
            }
        }
    }
    else if (gl_extensions && Q_strstr(gl_extensions, "GL_SGIS_multitexture "))
    {
        Con_DPrintf(ConLogType::Info, "Multitexture extensions found.\n");

        qglMTexCoord2fSGIS = (decltype(qglMTexCoord2fSGIS))sdl2()->SDL_GL_GetProcAddress("glMTexCoord2fSGIS");
        qglSelectTextureSGIS = (decltype(qglSelectTextureSGIS))sdl2()->SDL_GL_GetProcAddress("glSelectTextureSGIS");

        TEXTURE0_SGIS = QGL_TEXTURE0_SGIS;
        TEXTURE1_SGIS = QGL_TEXTURE1_SGIS;
        TEXTURE2_SGIS = QGL_TEXTURE2_SGIS;

        oldtarget = TEXTURE0_SGIS;
        *p_gl_mtexable = 2;
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

void GL_Init_Pre()
{
    gDisableMultitexture = g_UserConfig->get_value_int("disable_multitexture", 0);

    if (gDisableMultitexture)
    {
        V_strcpy_safe(*eng()->data_arb_multitexture, "fake_extension 0000 ");
    }
}

void GL_Init_Post()
{
    CheckMultiTextureExtensions();
    CheckTextureExtensions();
}