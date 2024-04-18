#pragma once

#include "HudBase.h"
#include "HudBaseHelper.h"

class HudHealth : public HudBase, public HudBaseHelper
{    
    int sprite_cross_width_, sprite_cross_height_;
    HSPRITE_t sprite_cross_handle_;

    float& m_fFade;
    int& m_iHealth;

public:   
    explicit HudHealth(nitroapi::NitroApiInterface* nitro_api);
    ~HudHealth() override;

    void Init() override;
    void VidInit() override;
    void Draw(float flTime) override;;
};
