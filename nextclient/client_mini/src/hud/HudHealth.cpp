#include "HudHealth.h"

#include "main.h"

#include <data_types/MessageReader.h>
#include <ncl_math/color.h>
#include <goldsrc/client_api.h>
#include <goldsrc/cstrike/game.h>
#include <goldsrc/cstrike/hud.h>
#include <goldsrc/cstrike/hud_elements.h>

#include "DrawUtils.h"

constexpr int MAX_HEALTH_TO_RENDER = 99999999;
constexpr int MAX_HEALTH_DIGITS_TO_RENDER = 8;

HudHealth::HudHealth(nitroapi::NitroApiInterface* nitro_api)
	: HudBaseHelper(nitro_api)
	, m_fFade(gHUD()->m_Health->m_fFade)
	, m_iHealth(gHUD()->m_Health->m_iHealth)
 {
	DeferUnsub(cl()->CHudHealth__Draw |= [this](goldsrc::cstrike::CHudHealth* const ptr, float flTime, const auto& next) {
		return 1;
	});
}

HudHealth::~HudHealth() {}

void HudHealth::Draw(float flTime) {
	if (m_iHideHUDDisplay & goldsrc::cstrike::HIDEHUD_HEALTH || cl_enginefunc()->IsSpectateOnly())
		return;

    if (m_fPlayerDead || ~m_iWeaponBits & (1 << (goldsrc::cstrike::WEAPON_SUIT))) 
		return;

	int health = std::min(m_iHealth, MAX_HEALTH_TO_RENDER);

	int width, height;
	GetScreenResolution(width, height);

    int x = 10;
    int y = height - 15 - m_iFontHeight;

    int a;
    ncl_math::Color color;

	if (health > 25)
		color = DrawUtils::UnpackRGB(goldsrc::cstrike::RGB_YELLOWISH);
	else {
		color = {255, 0, 0, 255};
	}

	if (m_fFade) {
		m_fFade -= (m_flTimeDelta * 20);
		if (m_fFade <= 0) {
			a = goldsrc::cstrike::MIN_ALPHA;
			m_fFade = 0;
		}
		a = goldsrc::cstrike::MIN_ALPHA + (m_fFade / goldsrc::cstrike::FADE_TIME) * 128;
	}
	else
		a = goldsrc::cstrike::MIN_ALPHA;

	color = DrawUtils::ScaleColors(color, a);

    SPR_Set(gHUD()->GetSprite(sprite_cross_handle_), color.r, color.g, color.b);
    SPR_DrawAdditive(0, x, y + abs(m_iFontHeight - sprite_cross_height_) / 2, &gHUD()->GetSpriteRect(sprite_cross_handle_));

    x += sprite_cross_width_ + 2;
    DrawUtils::DrawHudNumber(x, y, health, color, MAX_HEALTH_DIGITS_TO_RENDER, FALSE);
}

void HudHealth::VidInit() {
	sprite_cross_handle_ = gHUD()->m_Health->m_HUD_cross;
   	sprite_cross_width_ = gHUD()->GetSpriteWidth(sprite_cross_handle_);
    sprite_cross_height_ = gHUD()->GetSpriteHeight(sprite_cross_handle_);
}

static int MsgFunc_HealthNext(const char* pszName, int iSize, void* pbuf) {
	MessageReader reader(pbuf, iSize);
	int health = reader.ReadLong();

	gHUD->m_Health->m_iFlags |= 1u;
	if (health != gHUD->m_Health->m_iHealth) {
		gHUD->m_Health->m_fFade = goldsrc::cstrike::FADE_TIME;
		gHUD->m_Health->m_iHealth = health;
	}

	return 1;
}

void HudHealth::Init() {
	gEngfuncs.pfnHookUserMsg("HealthNEx", MsgFunc_HealthNext);
}