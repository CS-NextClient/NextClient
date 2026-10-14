#pragma once

#include <goldsrc/client_api.h>
#include <goldsrc/cstrike/game.h>
#include <goldsrc/cstrike/hud.h>
#include <goldsrc/cstrike/hud_elements.h>

#include "HudBase.h"
#include "HudBaseHelper.h"

class HudDamageIcons : public HudBase, public HudBaseHelper {
	int sprite_width_;
	int sprite_height_;
	goldsrc::HSPRITE_t sprite_handle_;

	int damage_flags_[goldsrc::cstrike::NUM_DMG_TYPES] = {goldsrc::cstrike::DMG_POISON,
		goldsrc::cstrike::DMG_ACID,
		goldsrc::cstrike::DMG_FREEZE | goldsrc::cstrike::DMG_SLOWFREEZE,
		goldsrc::cstrike::DMG_DROWN,
		goldsrc::cstrike::DMG_BURN | goldsrc::cstrike::DMG_SLOWBURN,
		goldsrc::cstrike::DMG_NERVEGAS,
		goldsrc::cstrike::DMG_RADIATION,
		goldsrc::cstrike::DMG_SHOCK,
		goldsrc::cstrike::DMG_CALTROP,
		goldsrc::cstrike::DMG_TRANQ,
		goldsrc::cstrike::DMG_CONCUSS,
		goldsrc::cstrike::DMG_HALLUC
	};

	int &m_bitsDamage;
	goldsrc::cstrike::DAMAGE_IMAGE* m_dmg;

public:
	explicit HudDamageIcons(nitroapi::NitroApiInterface* nitro_api);

	void Init() override;
	void VidInit() override;
	void Draw(float flTime) override;
};