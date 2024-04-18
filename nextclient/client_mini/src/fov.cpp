#include "main.h"
#include "hlsdk.h"
#include "fov.h"
#include "parsemsg.h"

static int MsgFunc_SetFOVEx(const char *pszName, int iSize, void *pbuf);

cvar_t* fov_horplus;
cvar_t* fov_lerp;
cvar_t* fov_angle;

float fovDifference = 1;

float currentFov = kFovDefault;
float destFov = kFovDefault;
float initialFov = kFovDefault;
float fovTime;
float fovLerp;

static float CalcCurrentFov()
{
    float w, h;
    SCREENINFO scr;

    scr.iSize = sizeof(SCREENINFO);
    gEngfuncs.pfnGetScreenInfo(&scr);

    w = (float)scr.iWidth;
    h = (float)scr.iHeight;

    float fovOffset = std::clamp(fov_angle->value, kFovMin, kFovMax) - kFovDefault;

    if (fov_horplus->value && ((float) (w / h) != 0.75f))
        return RAD2DEG(atan(tan(DEG2RAD(currentFov + fovOffset) / 2) * (w / h * 0.75f))) * 2;

    return currentFov + fovOffset;
}

static void SetFov(float fov)
{
    fovDifference = fov / kFovDefault;
    currentFov = fov;
}

static void ForceDestFov()
{
    initialFov = destFov;
    SetFov(destFov);
}

static float FovInterp(float a, float b, float f)
{
    f -= 1;
    return (b - a) * sqrt(1.0f - f * f) + a;
}

void FovInit()
{
    fov_horplus = gEngfuncs.pfnRegisterVariable("fov_horplus", "0", FCVAR_ARCHIVE);
    fov_lerp = gEngfuncs.pfnRegisterVariable("fov_lerp", "0", FCVAR_ARCHIVE);
    fov_angle = gEngfuncs.pfnRegisterVariable("fov_angle", "90", FCVAR_ARCHIVE);

    gEngfuncs.pfnHookUserMsg("SetFOVEx", MsgFunc_SetFOVEx);
}

void FovThink()
{
    float f, fov;
    float clientTime = gEngfuncs.GetClientTime();

    if ((int)destFov == (int)initialFov)
        return;

    if (!fovLerp)
    {
        ForceDestFov();
        return;
    }

    /* if player disconnects and reconnects, don't break the fov */
    if (clientTime < fovTime)
    {
        ForceDestFov();
        return;
    }

    f = (clientTime - fovTime) / fovLerp;

    if (f >= 1)
    {
        ForceDestFov();
        return;
    }

    fov = FovInterp(initialFov, destFov, f);
    SetFov(fov);
}

static void SetLerpFov(int fov, float lerp)
{
    if ((int)destFov == (int)fov)
        return;

    fovLerp = lerp;
    initialFov = currentFov;
    fovTime = gEngfuncs.GetClientTime();
    destFov = (float)fov;
}

static int MsgFunc_SetFOVEx(const char *pszName, int iSize, void *pbuf)
{
    BEGIN_READ(pbuf, iSize);
    int fov = READ_BYTE();
    float lerp = READ_FLOAT();

    if (fov < 1 || fov > 180)
        fov = kFovDefault;

    SetLerpFov(fov, lerp);

    return 1;
}

int FovMsgFunc_SetFOV(const char *pszName, int iSize, void *pbuf, UserMsg_SetFOVNext next)
{
    BEGIN_READ(pbuf, iSize);
    int fov = READ_BYTE();

    if (fov < 1 || fov > 180)
        fov = kFovDefault;

    SetLerpFov(fov, std::clamp(fov_lerp->value, 0.f, 0.8f));

    return next->Invoke(pszName, iSize, pbuf);
}

void FovHUD_UpdateClientData(client_data_t *cdata, float flTime, int result)
{
    cdata->fov = CalcCurrentFov();
}
