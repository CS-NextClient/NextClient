#include "HudRadar.h"
#include <goldsrc/client_api.h>
#include <goldsrc/cstrike/game.h>
#include <goldsrc/cstrike/hud_elements.h>

#include "HudRadarAdvanced.h"
#include "hud/radar/integration/RadarConfig.h"

HudRadar::HudRadar(nitroapi::NitroApiInterface* nitro_api, RadarDrawContext& draw) :
    HudBaseHelper(nitro_api),
    draw_(draw),
    m_bDrawRadar(gHUD()->m_Health->m_bDrawRadar)
{}

void HudRadar::Draw(float flTime) {
	// Yield to the advanced radar unless it had no map to draw this frame, in which case
	// neither radar would render and the corner would sit blank.
    if ((RadarConfig_IsAdvancedSelected() && draw_.are_prims_ready()) && !HudRadarAdvanced::is_map_unavailable())
    {
        return;
    }

    if (m_iHideHUDDisplay & goldsrc::cstrike::HIDEHUD_HEALTH || cl_enginefunc()->IsSpectateOnly())
    {
        return;
    }

    GetAllPlayersInfo();

    if(!m_fPlayerDead && m_bDrawRadar)
        cl()->CHudHealth__DrawRadar(gHUD()->m_Health, flTime);
}