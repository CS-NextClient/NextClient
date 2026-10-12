#pragma once

#include "nitroapi/NitroApiInterface.h"
#include "nitroapi/NitroApiHelper.h"
#include "HudBase.h"
#include <ncl_math/color.h>
#include <goldsrc/client_api.h>
#include <goldsrc/cvar.h>

class HudCrosshair : public HudBase, public nitroapi::NitroApiHelper
{
    std::vector<std::shared_ptr<nitroapi::Unsubscriber>> unsubscribers_;

    goldsrc::SCREENINFO screeninfo_;

    goldsrc::cvar_t* cl_crosshair_type_;
    goldsrc::cvar_t* cl_dynamiccrosshair_;
    goldsrc::cvar_t* cl_crosshair_color_;
    goldsrc::cvar_t* cl_crosshair_size_;
    goldsrc::cvar_t* cl_crosshair_translucent_;

    ncl_math::Color m_color{};
    // Raw (unclamped) parsed cvar channels, kept as the change-detection key: a
    // Color would truncate out-of-range channels mod 256 and miss real changes.
    int m_cvarR{}, m_cvarG{}, m_cvarB{};
    int m_bAdditive{};
    char m_szLastCrosshairColor[16]{};
    char m_szLastCrosshairSize[16]{};
    int m_iAmmoLastCheck{};
    float m_flCrosshairDistance{};
    int m_iCrosshairScaleBase{};

public:
    explicit HudCrosshair(nitroapi::NitroApiInterface* nitro_api);
    ~HudCrosshair() override;

    void Init() override;
    void VidInit() override;

private:
    int CenterX() const;

    void DrawCrosshair(float flTime, int weaponid);
    int GetWeaponAccuracyFlags(int iWeaponID);

    void CalculateCrosshairSize();
    void CalculateCrosshairColor();
    void CalculateCrosshairDrawMode();

    void DrawCrosshairEx(int iBarSize, float flCrosshairDistance, bool bAdditive, ncl_math::Color color);
};
