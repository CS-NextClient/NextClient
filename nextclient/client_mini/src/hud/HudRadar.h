#pragma once

class RadarDrawContext;

#include "HudBase.h"
#include "HudBaseHelper.h"

class HudRadar : public HudBase, public HudBaseHelper {
    RadarDrawContext& draw_;
    bool& m_bDrawRadar;

public:
    explicit HudRadar(nitroapi::NitroApiInterface* nitro_api, RadarDrawContext& draw);

    void Draw(float flTime) override;
};