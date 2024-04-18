#pragma once

#include "HudBase.h"
#include "HudBaseHelper.h"

class HudRadar : public HudBase, public HudBaseHelper {
	bool &m_bDrawRadar;
	
public:
	explicit HudRadar(nitroapi::NitroApiInterface* nitro_api);

	void Draw(float flTime) override;
};