#include "HudRadar.h"
#include "HudRadarAdvanced.h"
#include "radar/RadarConfig.h"

HudRadar::HudRadar(nitroapi::NitroApiInterface* nitro_api)
	: HudBaseHelper(nitro_api)
	, m_bDrawRadar(gHUD()->m_Health->m_bDrawRadar) {}

void HudRadar::Draw(float flTime) {
	// Yield to the advanced radar unless it had no map to draw this frame, in which case
	// neither radar would render and the corner would sit blank.
	if (RadarConfig_CheckAdvancedActive() && !HudRadarAdvanced::is_map_unavailable())
	{
		return;
	}

	if (m_iHideHUDDisplay & HIDEHUD_HEALTH || cl_enginefunc()->IsSpectateOnly())
		return;

    GetAllPlayersInfo();

    if(!m_fPlayerDead && m_bDrawRadar)
        cl()->CHudHealth__DrawRadar(gHUD()->m_Health, flTime);
}