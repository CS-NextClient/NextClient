#pragma once

#include "HudBase.h"
#include "HudBaseHelper.h"

class HudDamageIcons : public HudBase, public HudBaseHelper {
	int sprite_width_;
	int sprite_height_;
	HSPRITE_t sprite_handle_;

	int damage_flags_[NUM_DMG_TYPES] = {
		DMG_POISON,
		DMG_ACID,
		DMG_FREEZE|DMG_SLOWFREEZE,
		DMG_DROWN,
		DMG_BURN|DMG_SLOWBURN,
		DMG_NERVEGAS,
		DMG_RADIATION,
		DMG_SHOCK,
		DMG_CALTROP,
		DMG_TRANQ,
		DMG_CONCUSS,
		DMG_HALLUC
	};

	int &m_bitsDamage;
	DAMAGE_IMAGE *m_dmg;

public:
	explicit HudDamageIcons(nitroapi::NitroApiInterface* nitro_api);

	void Init() override;
	void VidInit() override;
	void Draw(float flTime) override;
};