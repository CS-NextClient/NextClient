#pragma once

#include "nitroapi/NitroApiInterface.h"
#include "nitroapi/NitroApiHelper.h"
#include "HudBase.h"

enum class CrossHairType
{
    Cross,
    T,
    Circle,
    Dot,
    END_VAL
};

class HudCrosshair : public HudBase, public nitroapi::NitroApiHelper
{
    std::vector<std::shared_ptr<nitroapi::Unsubscriber>> unsubscribers_;

    SCREENINFO_s screeninfo_;

    cvar_t* cl_crosshair_type_;
    cvar_t* cl_dynamiccrosshair_;
    cvar_t* cl_crosshair_color_;
    cvar_t* cl_crosshair_size_;
    cvar_t* cl_crosshair_translucent_;

    int m_R{}, m_G{}, m_B{};
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
    void DrawCrosshair(float flTime, int weaponid);
    int GetWeaponAccuracyFlags(int iWeaponID);

    void CalculateCrosshairSize();
    void CalculateCrosshairColor();
    void CalculateCrosshairDrawMode();

    void DrawCrosshairEx(int iBarSize, float flCrosshairDistance, bool bAdditive, int r, int g, int b, int a);
};
