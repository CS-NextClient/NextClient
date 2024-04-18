#include "HudHealth.h"
#include "../main.h"
#include <parsemsg.h>
#include "DrawUtils.h"

constexpr int MAX_HEALTH_TO_RENDER = 99999999;
constexpr int MAX_HEALTH_DIGITS_TO_RENDER = 8;

HudHealth::HudHealth(nitroapi::NitroApiInterface* nitro_api)
	: HudBaseHelper(nitro_api)
	, m_fFade(gHUD()->m_Health->m_fFade)
	, m_iHealth(gHUD()->m_Health->m_iHealth)
 {
	DeferUnsub(cl()->CHudHealth__Draw |= [this](CHudHealth* const ptr, float flTime, const auto& next) {
		return 1;
	});
}

HudHealth::~HudHealth() {}

void HudHealth::Draw(float flTime) {
	if (m_iHideHUDDisplay & HIDEHUD_HEALTH || cl_enginefunc()->IsSpectateOnly())
		return;

    if(m_fPlayerDead || ~m_iWeaponBits & (1<<(WEAPON_SUIT))) 
		return;

	int health = std::min(m_iHealth, MAX_HEALTH_TO_RENDER);

	int width, height;
	GetScreenResolution(width, height);

    int x = 10;
    int y = height - 15 - m_iFontHeight;

    int r, g, b, a;

	if(health > 25) DrawUtils::UnpackRGB(r, g, b, RGB_YELLOWISH);
	else {
		r = 255;
        g = b = 0;
	}

	if (m_fFade) {
		m_fFade -= (m_flTimeDelta * 20);
		if (m_fFade <= 0) {
			a = MIN_ALPHA;
			m_fFade = 0;
		}
		a = MIN_ALPHA + (m_fFade / FADE_TIME) * 128;
	}
	else
		a = MIN_ALPHA;
	
	DrawUtils::ScaleColors(r, g, b, a);

    SPR_Set(gHUD()->GetSprite(sprite_cross_handle_), r, g, b);
    SPR_DrawAdditive(0, x, y + abs(m_iFontHeight - sprite_cross_height_) / 2, &gHUD()->GetSpriteRect(sprite_cross_handle_));

    x += sprite_cross_width_ + 2;
    DrawUtils::DrawHudNumber(x, y, health, r, g, b, MAX_HEALTH_DIGITS_TO_RENDER, FALSE);
}

void HudHealth::VidInit() {
	sprite_cross_handle_ = gHUD()->m_Health->m_HUD_cross;
   	sprite_cross_width_ = gHUD()->GetSpriteWidth(sprite_cross_handle_);
    sprite_cross_height_ = gHUD()->GetSpriteHeight(sprite_cross_handle_);
}

static int MsgFunc_HealthNext(const char* pszName, int iSize, void* pbuf) {
	BEGIN_READ(pbuf, iSize);
	int health = READ_LONG();

	gHUD->m_Health->m_iFlags |= 1u;
	if (health != gHUD->m_Health->m_iHealth) {
		gHUD->m_Health->m_fFade = FADE_TIME;
		gHUD->m_Health->m_iHealth = health;
	}

	return 1;
}

void HudHealth::Init() {
	gEngfuncs.pfnHookUserMsg("HealthNEx", MsgFunc_HealthNext);
}