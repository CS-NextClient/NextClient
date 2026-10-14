#pragma once

#include <ncl_math/vec3.h>
#include <goldsrc/client_api.h>

#include "HudBase.h"
#include "HudBaseHelper.h"

enum {
	ATK_FRONT = 0,
	ATK_REAR,
	ATK_LEFT,
	ATK_RIGHT,

	ATK_ENUM_END
};

class HudDamageDirection : public HudBase, public HudBaseHelper {
	float* attack_indicators_;
	goldsrc::HSPRITE_t& sprite_handle_;
	ncl_math::Vector2 sprite_positions_[ATK_ENUM_END];

	int sprite_frames_[ATK_ENUM_END] = {
		0, 2, 3, 1
	};

public:
	explicit HudDamageDirection(nitroapi::NitroApiInterface* nitro_api);

	void Init() override;
	void VidInit() override;
	void Draw(float flTime) override;
};