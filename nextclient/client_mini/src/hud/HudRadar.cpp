#include "HudRadar.h"

HudRadar::HudRadar(nitroapi::NitroApiInterface* nitro_api) 
	: HudBaseHelper(nitro_api)
	, m_bDrawRadar(gHUD()->m_Health->m_bDrawRadar) {}

void HudRadar::Draw(float flTime) {
	if (m_iHideHUDDisplay & HIDEHUD_HEALTH || cl_enginefunc()->IsSpectateOnly())
		return;

    GetAllPlayersInfo();

    if(!m_fPlayerDead && m_bDrawRadar)
        cl()->CHudHealth__DrawRadar(gHUD()->m_Health, flTime);
}