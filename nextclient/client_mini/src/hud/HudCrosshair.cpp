#include "HudCrosshair.h"
#include "weapontype.h"
#include "../shared_util.h"
#include "../main.h"

HudCrosshair::HudCrosshair(nitroapi::NitroApiInterface *nitro_api) :
    nitroapi::NitroApiHelper(nitro_api),
    screeninfo_{sizeof(SCREENINFO)}
{
    unsubscribers_.emplace_back(cl()->CHudAmmo__DrawCrosshair |= [this](CHudAmmo* const ptr, float time, int weaponid, const auto& next) {
        DrawCrosshair(time, weaponid);
        return 1;
    });
}

HudCrosshair::~HudCrosshair()
{
    for (auto& unsubscriber : unsubscribers_)
    {
        unsubscriber->Unsubscribe();
    }
}

void HudCrosshair::Init()
{
    cl_crosshair_type_ = cl_enginefunc()->pfnRegisterVariable("cl_crosshair_type", "0", FCVAR_ARCHIVE);
    cl_crosshair_color_ = cl_enginefunc()->pfnGetCvarPointer("cl_crosshair_color");
    cl_crosshair_size_ = cl_enginefunc()->pfnGetCvarPointer("cl_crosshair_size");
    cl_crosshair_translucent_ = cl_enginefunc()->pfnGetCvarPointer("cl_crosshair_translucent");
    cl_dynamiccrosshair_ = cl_enginefunc()->pfnGetCvarPointer("cl_dynamiccrosshair");
}

void HudCrosshair::VidInit()
{
    cl_enginefunc()->pfnGetScreenInfo(&screeninfo_);

    m_flPrevCrosshairTime = 0;
    m_flPrevTime = 0;
    m_iAmmoLastCheck = 0;
    m_flCrosshairDistance = 0;
    m_iCrosshairScaleBase = 0;
    m_szLastCrosshairColor[0] = '\0';
}

void HudCrosshair::DrawCrosshair(float flTime, int weaponid)
{
    int iDistance;
    int iDeltaDistance;
    float flCurTime = cl_enginefunc()->GetClientTime();

    switch (weaponid)
    {
        case WEAPON_P228:
        case WEAPON_HEGRENADE:
        case WEAPON_SMOKEGRENADE:
        case WEAPON_FIVESEVEN:
        case WEAPON_USP:
        case WEAPON_GLOCK18:
        case WEAPON_AWP:
        case WEAPON_FLASHBANG:
        case WEAPON_DEAGLE:
        {
            iDistance = 8;
            iDeltaDistance = 3;
            break;
        }
        case WEAPON_MP5N:
        {
            iDistance = 6;
            iDeltaDistance = 2;
            break;
        }
        case WEAPON_M3:
        {
            iDistance = 8;
            iDeltaDistance = 6;
            break;
        }
        case WEAPON_G3SG1:
        {
            iDistance = 6;
            iDeltaDistance = 4;
            break;
        }
        case WEAPON_AK47:
        {
            iDistance = 4;
            iDeltaDistance = 4;
            break;
        }
        case WEAPON_TMP:
        case WEAPON_KNIFE:
        case WEAPON_P90:
        {
            iDistance = 7;
            iDeltaDistance = 3;
            break;
        }
        case WEAPON_XM1014:
        {
            iDistance = 9;
            iDeltaDistance = 4;
            break;
        }
        case WEAPON_MAC10:
        {
            iDistance = 9;
            iDeltaDistance = 3;
            break;
        }
        case WEAPON_AUG:
        {
            iDistance = 3;
            iDeltaDistance = 3;
            break;
        }
        case WEAPON_C4:
        case WEAPON_UMP45:
        case WEAPON_M249:
        {
            iDistance = 6;
            iDeltaDistance = 4;
            break;
        }
        case WEAPON_SCOUT:
        case WEAPON_SG550:
        case WEAPON_SG552:
        {
            iDistance = 5;
            iDeltaDistance = 3;
            break;
        }
        default:
        {
            iDistance = 4;
            iDeltaDistance = 3;
            break;
        }
    }

    int accuracy_flags = GetWeaponAccuracyFlags(weaponid);
    if (accuracy_flags && cl_dynamiccrosshair_ && cl_dynamiccrosshair_->value != 0.0 && !(*cl()->gHUD->m_iHideHUDDisplay & 1))
    {
        if (g_LastPlayerState.client.flags & FL_ONGROUND || !(accuracy_flags & 1))
        {
            if (g_LastPlayerState.client.flags & FL_DUCKING && accuracy_flags & 4)
            {
                iDistance *= 0.5f;
            }
            else
            {
                float flLimitSpeed;

                switch (weaponid)
                {
                    case WEAPON_AUG:
                    case WEAPON_M4A1:
                    case WEAPON_FAMAS:
                    case WEAPON_SG550:
                    case WEAPON_GALIL:
                    case WEAPON_AK47:
                    case WEAPON_M249:
                    case WEAPON_SG552:
                    {
                        flLimitSpeed = 140;
                        break;
                    }
                    case WEAPON_P90:
                    {
                        flLimitSpeed = 170;
                        break;
                    }
                    default:
                    {
                        flLimitSpeed = 0;
                        break;
                    }
                }
                if (Vector(g_LastPlayerState.client.velocity).Length() > flLimitSpeed && (accuracy_flags & 2))
                    iDistance *= 1.5f;
            }
        }
        else
        {
            iDistance *= 2;
        }
        if ( accuracy_flags & 8 )
        {
            iDistance *= 1.4f;
        }
        if ( accuracy_flags & 16 )
        {
            iDistance *= 1.4f;
        }
    }

    if (*cl()->g_iShotsFired > m_iAmmoLastCheck)
    {
        m_flCrosshairDistance += (float)iDeltaDistance;

        if (m_flCrosshairDistance > 15.0)
            m_flCrosshairDistance = 15.0;
    }
    else
    {
        m_flCrosshairDistance -= 1.3f * (flCurTime - m_flPrevCrosshairTime) * m_flCrosshairDistance;
    }

    m_flPrevCrosshairTime = flCurTime;

    if (*cl()->g_iShotsFired > 600)
        *cl()->g_iShotsFired = 1;

    m_iAmmoLastCheck = *cl()->g_iShotsFired;

    if ((float)iDistance > m_flCrosshairDistance)
        m_flCrosshairDistance = (float)iDistance;

    int iBarSize = (m_flCrosshairDistance - iDistance) * 0.5f + 5;

    if (flCurTime - m_flPrevTime > 1.0f)
    {
        CalculateCrosshairColor();
        CalculateCrosshairDrawMode();
        CalculateCrosshairSize();
        m_flPrevTime = flCurTime;
    }

    float flCrosshairDistance = m_flCrosshairDistance;
    if (screeninfo_.iWidth != m_iCrosshairScaleBase)
    {
        flCrosshairDistance = m_flCrosshairDistance * (float)screeninfo_.iWidth / (float)m_iCrosshairScaleBase;
        iBarSize = screeninfo_.iWidth * iBarSize / m_iCrosshairScaleBase;
    }

    if (cl()->gHUD->m_NightVision->m_fOn)
        DrawCrosshairEx(iBarSize, flCrosshairDistance, false, 250, 50, 50, 255);
    else
        DrawCrosshairEx(iBarSize, flCrosshairDistance, m_bAdditive, m_R, m_G, m_B, 255);
}

int HudCrosshair::GetWeaponAccuracyFlags(int iWeaponID)
{
    weapon_data_t* weapondata = &g_LastPlayerState.weapondata[g_LastPlayerState.client.m_iId];

    int flags;

    switch (iWeaponID)
    {
        case WEAPON_USP:
        {
            flags = (weapondata->m_iWeaponState & WPNSTATE_USP_SILENCED) < 1 ? 7 : 15;
            break;
        }
        case WEAPON_GLOCK18:
        {
            flags = (weapondata->m_iWeaponState & WPNSTATE_GLOCK18_BURST_MODE) < 1 ? 7 : 23;
            break;
        }
        case WEAPON_M4A1:
        {
            flags = (weapondata->m_iWeaponState & WPNSTATE_M4A1_SILENCED) < 1 ? 3 : 11;
            break;
        }
        case WEAPON_FAMAS:
        {
            flags = (weapondata->m_iWeaponState & WPNSTATE_FAMAS_BURST_MODE) < 1 ? 3 : 19;
            break;
        }
        case WEAPON_MAC10:
        case WEAPON_UMP45:
        case WEAPON_MP5N:
        case WEAPON_TMP:
        {
            flags = 1;
            break;
        }
        case WEAPON_AUG:
        case WEAPON_GALIL:
        case WEAPON_M249:
        case WEAPON_SG552:
        case WEAPON_AK47:
        case WEAPON_P90:
        {
            flags = 3;
            break;
        }
        case WEAPON_P228:
        case WEAPON_FIVESEVEN:
        case WEAPON_DEAGLE:
        {
            flags = 7;
            break;
        }
        default:
        {
            flags = 0;
            break;
        }
    }

    return flags;
}

void HudCrosshair::CalculateCrosshairSize()
{
    char* value = cl_crosshair_size_->string;

    if (!value)
        return;

    int size = -1;

    if (!stricmp(value, "auto"))
        size = 0;
    else if (!stricmp(value, "small"))
        size = 1;
    else if (!stricmp(value, "medium"))
        size = 2;
    else if (!stricmp(value, "large"))
        size = 3;
    else if (!stricmp(value, "extra_small"))
        size = 4;

    switch (size)
    {
        default:
        case 0:
        {
            if (screeninfo_.iWidth >= 1024)
                m_iCrosshairScaleBase = 640;
            else if (screeninfo_.iWidth >= 800)
                m_iCrosshairScaleBase = 800;
            else
                m_iCrosshairScaleBase = 1024;

            break;
        }
        case 1:
        {
            m_iCrosshairScaleBase = 1024;
            break;
        }
        case 2:
        {
            m_iCrosshairScaleBase = 800;
            break;
        }
        case 3:
        {
            m_iCrosshairScaleBase = 640;
            break;
        }
        case 4:
        {
            m_iCrosshairScaleBase = 1400;
            break;
        }
    }
}

void HudCrosshair::CalculateCrosshairColor()
{
    char *value = cl_crosshair_color_->string;

    if (value && strcmp(value, m_szLastCrosshairColor) != 0)
    {
        int cvarR, cvarG, cvarB;
        char *token;
        char *data = value;

        data = SharedParse(data);
        token = SharedGetToken();

        if (token)
        {
            cvarR = atoi(token);

            data = SharedParse(data);
            token = SharedGetToken();

            if (token)
            {
                cvarG = atoi(token);

                data = SharedParse(data);
                token = SharedGetToken();

                if (token)
                {
                    cvarB = atoi(token);

                    if (m_cvarR != cvarR || m_cvarG != cvarG || m_cvarB != cvarB)
                    {
                        int r, g, b;

                        r = std::clamp(cvarR, 0, 255);
                        g = std::clamp(cvarG, 0, 255);
                        b = std::clamp(cvarB, 0, 255);

                        m_R = r;
                        m_G = g;
                        m_B = b;
                        m_cvarR = cvarR;
                        m_cvarG = cvarG;
                        m_cvarB = cvarB;
                    }

                    V_strcpy_safe(m_szLastCrosshairColor, value);
                }
            }
        }
    }
}

void HudCrosshair::CalculateCrosshairDrawMode()
{
    float value = cl_crosshair_translucent_->value;

    if (value == 0)
        m_bAdditive = false;
    else if (value == 1)
        m_bAdditive = true;
    else
        Con_Printf("usage: cl_crosshair_translucent <1|0>\n");
}

void HudCrosshair::DrawCrosshairEx(int iBarSize, float flCrosshairDistance, bool bAdditive, int r, int g, int b, int a)
{
    auto eCrosshairType = (CrossHairType)std::clamp((int)cl_crosshair_type_->value, 0, (int)CrossHairType::END_VAL - 1);

    void (*pfnFillRGBA)(int x, int y, int w, int h, int r, int g, int b, int a) = bAdditive ? cl_enginefunc()->pfnFillRGBA : cl_enginefunc()->pfnFillRGBABlend;

    if (eCrosshairType == CrossHairType::Circle)
    {
        float radius = (iBarSize / 2) + flCrosshairDistance;
        int count = (int)((cos(M_PI / 4) * radius) + 0.5);

        for (int i = 0; i < count; i++)
        {
            int size = sqrt((radius * radius) - (float)(i * i));

            pfnFillRGBA((screeninfo_.iWidth / 2) + i, (screeninfo_.iHeight / 2) + size, 1, 1, r, g, b, a);
            pfnFillRGBA((screeninfo_.iWidth / 2) + i, (screeninfo_.iHeight / 2) - size, 1, 1, r, g, b, a);
            pfnFillRGBA((screeninfo_.iWidth / 2) - i, (screeninfo_.iHeight / 2) + size, 1, 1, r, g, b, a);
            pfnFillRGBA((screeninfo_.iWidth / 2) - i, (screeninfo_.iHeight / 2) - size, 1, 1, r, g, b, a);
            pfnFillRGBA((screeninfo_.iWidth / 2) + size, (screeninfo_.iHeight / 2) + i, 1, 1, r, g, b, a);
            pfnFillRGBA((screeninfo_.iWidth / 2) + size, (screeninfo_.iHeight / 2) - i, 1, 1, r, g, b, a);
            pfnFillRGBA((screeninfo_.iWidth / 2) - size, (screeninfo_.iHeight / 2) + i, 1, 1, r, g, b, a);
            pfnFillRGBA((screeninfo_.iWidth / 2) - size, (screeninfo_.iHeight / 2) - i, 1, 1, r, g, b, a);
        }
    }
    else if (eCrosshairType == CrossHairType::Cross || eCrosshairType == CrossHairType::T)
    {
        pfnFillRGBA((screeninfo_.iWidth / 2) + (int)flCrosshairDistance, screeninfo_.iHeight / 2, iBarSize, 1, r, g, b, a);
        pfnFillRGBA((screeninfo_.iWidth / 2) - (int)flCrosshairDistance - iBarSize + 1, screeninfo_.iHeight / 2, iBarSize, 1, r, g, b, a);
        pfnFillRGBA(screeninfo_.iWidth / 2, (screeninfo_.iHeight / 2) + (int)flCrosshairDistance, 1, iBarSize, r, g, b, a);
        if (eCrosshairType != CrossHairType::T)
            pfnFillRGBA(screeninfo_.iWidth / 2, (screeninfo_.iHeight / 2) - (int)flCrosshairDistance - iBarSize + 1, 1, iBarSize, r, g, b, a);

    }
    else if (eCrosshairType == CrossHairType::Dot)
    {
        pfnFillRGBA((screeninfo_.iWidth / 2) - 1, (screeninfo_.iHeight / 2) - 1, 3, 3, r, g, b, a);
    }
}
