#include "HudDamageDirection.h"

constexpr float EPSILON = 0.4f;
constexpr auto PAIN_NAME = "sprites/%d_pain.spr";

HudDamageDirection::HudDamageDirection(nitroapi::NitroApiInterface* nitro_api)
	: HudBaseHelper(nitro_api)
	, sprite_handle_(gHUD()->m_Health->m_hSprite)
	, attack_indicators_(&gHUD()->m_Health->m_fAttackFront) { }

void HudDamageDirection::Init() {
}

void HudDamageDirection::VidInit() {
	if (!sprite_handle_)
		sprite_handle_ = LoadSprite(PAIN_NAME);

	int width, height;
	GetScreenResolution(width, height);

	sprite_positions_[ATK_FRONT].x = width / 2.0 - SPR_Width(sprite_handle_, 0) / 2;
	sprite_positions_[ATK_FRONT].y = height / 2.0 - SPR_Height(sprite_handle_, 0) * 3;

	sprite_positions_[ATK_RIGHT].x = width / 2.0 + SPR_Width(sprite_handle_, 1) * 2;
	sprite_positions_[ATK_RIGHT].y = height / 2.0 - SPR_Height(sprite_handle_, 1) / 2;

	sprite_positions_[ATK_REAR].x = width / 2.0 - SPR_Width(sprite_handle_, 2) / 2;
	sprite_positions_[ATK_REAR].y = height / 2.0 + SPR_Height(sprite_handle_, 2) * 2;

	sprite_positions_[ATK_LEFT].x = width / 2.0 - SPR_Width(sprite_handle_, 3) * 3;
	sprite_positions_[ATK_LEFT].y = height / 2.0 - SPR_Height(sprite_handle_, 3) / 2;
}

void HudDamageDirection::Draw(float flTime) {
	if (m_iHideHUDDisplay & HIDEHUD_HEALTH || cl_enginefunc()->IsSpectateOnly())
		return;
		
	if (attack_indicators_[ATK_FRONT] == 0 && attack_indicators_[ATK_RIGHT] == 0
		&& attack_indicators_[ATK_REAR] == 0 && attack_indicators_[ATK_LEFT] == 0)
		return;

	float a, fade = m_flTimeDelta * 2;

	for (int i = 0; i < ATK_ENUM_END; i++) {
		if (attack_indicators_[i] > EPSILON) {
			a = std::max(attack_indicators_[i], 0.5f);

			SPR_Set(sprite_handle_, 255 * a, 255 * a, 255 * a);
			SPR_DrawAdditive(sprite_frames_[i], sprite_positions_[i].x, sprite_positions_[i].y, NULL);

			attack_indicators_[i] = std::max(0.0f, attack_indicators_[i] - fade);
		} else
			attack_indicators_[i] = 0;
	}
}
