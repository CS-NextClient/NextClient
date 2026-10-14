#include "HudDamageIcons.h"
#include "DrawUtils.h"
#include <ncl_math/color.h>
#include <goldsrc/client_api.h>
#include <goldsrc/cstrike/game.h>
#include <goldsrc/cstrike/hud.h>
#include <goldsrc/cstrike/hud_elements.h>

HudDamageIcons::HudDamageIcons(nitroapi::NitroApiInterface* nitro_api)
	: HudBaseHelper(nitro_api)
	, m_bitsDamage(gHUD()->m_Health->m_bitsDamage)
	, m_dmg(gHUD()->m_Health->m_dmg) { }

void HudDamageIcons::Init() {
}

void HudDamageIcons::VidInit() {
	sprite_handle_ = gHUD()->m_Health->m_HUD_dmg_bio;
	sprite_width_ = gHUD()->GetSpriteWidth(sprite_handle_);
	sprite_height_ = gHUD()->GetSpriteHeight(sprite_handle_);
}

void HudDamageIcons::Draw(float flTime) {
	if (m_iHideHUDDisplay & goldsrc::cstrike::HIDEHUD_HEALTH || cl_enginefunc()->IsSpectateOnly())
		return;
		
	if (!m_bitsDamage)
		return;

	size_t i;
	int a;

	ncl_math::Color color = DrawUtils::UnpackRGB(goldsrc::cstrike::RGB_YELLOWISH);

	a = (int)(fabs(sin(flTime * 2)) * 256.0);
	color = DrawUtils::ScaleColors(color, a);

	// Draw all the items
	for (i = 0; i < goldsrc::cstrike::NUM_DMG_TYPES; i++)
	{
		if (m_bitsDamage & damage_flags_[i]) {
			goldsrc::cstrike::DAMAGE_IMAGE* pdmg = &m_dmg[i];

			SPR_Set(gHUD()->GetSprite(gHUD()->m_Health->m_HUD_dmg_bio + i), color.r, color.g, color.b);
			SPR_DrawAdditive(0, pdmg->x, pdmg->y,
				&gHUD()->GetSpriteRect(gHUD()->m_Health->m_HUD_dmg_bio + i));
		}
	}

	// check for bits that should be expired
	for (i = 0; i < goldsrc::cstrike::NUM_DMG_TYPES; i++)
	{
		goldsrc::cstrike::DAMAGE_IMAGE* pdmg = &m_dmg[i];

		if (m_bitsDamage & damage_flags_[i]) {
			pdmg->fExpire = std::min(flTime + goldsrc::cstrike::DMG_IMAGE_LIFE, pdmg->fExpire);

			if (pdmg->fExpire <= flTime // when the time has expired
				&& a < 40) // and the flash is at the low point of the cycle
			{
				pdmg->fExpire = 0;

				int y = pdmg->y;
				pdmg->x = pdmg->y = 0;

				// move everyone above down
				for (size_t j = 0; j < goldsrc::cstrike::NUM_DMG_TYPES; j++)
				{
					pdmg = &gHUD()->m_Health->m_dmg[j];
					if ((pdmg->y) && (pdmg->y < y))
						pdmg->y += sprite_height_;
				}

				m_bitsDamage &= ~damage_flags_[i]; // clear the bits
			}
		}
	}
}
