#pragma once

#include "HudBase.h"
#include "HudBaseHelper.h"
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <memory>

enum DeathMessageFlags {
	// float[3]
	// Position where the victim was killed by the enemy
	PLAYERDEATH_POSITION = 0x001,

	// byte
	// Index of the assistant who helped the attacker kill the victim
	PLAYERDEATH_ASSISTANT = 0x002,

	// short
	// Bitsum classification for the rarity of the kill
	// See enum KillRarity for details
	PLAYERDEATH_KILLRARITY = 0x004
};

enum KillRarity {
	KILLRARITY_HEADSHOT         = 0x001, // Headshot
	KILLRARITY_KILLER_BLIND     = 0x002, // Killer was blind
	KILLRARITY_NOSCOPE          = 0x004, // No-scope sniper rifle kill
	KILLRARITY_PENETRATED       = 0x008, // Penetrated kill (through walls)
	KILLRARITY_THRUSMOKE        = 0x010, // Smoke grenade penetration kill (bullets went through smoke)
	KILLRARITY_ASSISTEDFLASH    = 0x020, // Assister helped with a flash
	KILLRARITY_DOMINATION_BEGAN = 0x040, // Killer player began dominating the victim (NOTE: this flag is set once)
	KILLRARITY_DOMINATION       = 0x080, // Continues domination by the killer
	KILLRARITY_REVENGE          = 0x100  // Revenge by the killer
};

class HudDeathNotice : public HudBase, public HudBaseHelper {
	cvar_t* cvar_deathnotice_time_;
	cvar_t* cvar_deathnotice_max_;
	cvar_t* cvar_deathnotice_old_;

	enum RarityFrame {
		HEADSHOT,
		KILLER_BLIND,
		NOSCOPE,
		PENETRATED,
		THROUGH_SMOKE,
		ASSIST_FLASH,
		DOMINATION,
		REVENGE
	};

public:	
	struct wpn_icon_override_t {
		HSPRITE_t sprite;
		int frame;
		int rendermode;
		vec3_t color;
		float alpha;
		float ideal_scale;
		int ideal_w; 
		int ideal_h;
	};

	struct notice_row_t {
		std::string killer_name;
		std::string victim_name;
		std::string assistant_name;
		int killer_id;
		float* killer_color;
		float* victim_color;
		float* assistant_color;
		float display_time;
		bool is_teamkill;
		bool is_should_kill_highlight;
		bool is_should_dead_highlight;
		KillRarity kill_rarity_flags;

		int weapon_sprite_index;
		wpn_icon_override_t custom_weapon_sprite{};
	};
	
private:
	std::vector<notice_row_t> notice_rows_;
	wpn_icon_override_t next_custom_weapon_sprite_{};
	
	HSPRITE_t kill_rarity_sprite_ {};
	float kill_rarity_sprite_scale_;
	float kill_rarity_sprite_alpha_;
	int kill_rarity_sprite_rendermode_;
	int skull_sprite_index_ {};
	int kill_rarity_sprite_width_, kill_rarity_sprite_height_;

	vec3_t sprite_icons_color_ = { 1.0, 1.0, 1.0 };
	int kill_rarity_sprite_padding_x_;
	int weapon_sprite_padding_x_;
	int string_padding_x_;

	int draw_string_font_height_;
	int notice_box_padding_top_, notice_box_padding_bottom_;
	int notice_box_padding_x_;
	int notice_box_height_;
	int notice_box_outline_width_;
	int notice_boxes_gap_;

	int DrawScaledSprite(
		HSPRITE_t* sprite, int frame,
		int x, int y, float scale,
		int rendermode, vec3_t color, float alpha);
	int DrawKillRaritySprite(RarityFrame type, int x, int y);
	int DrawWeaponSprite(int index, int x, int y);
	int DrawString(const char* text, vec3_t color, int x, int y);

	int GetKillRaritySpriteFullWidth();
	int GetWeaponSpriteFullWidth(int index);
	int GetStringFullWidth(const char* text);

	int GetCustomWeaponSpriteFullWidth(wpn_icon_override_t* icon);
	int GetCustomWeaponSpriteHeight(wpn_icon_override_t* icon);
	int DrawCustomWeaponSprite(wpn_icon_override_t* icon, int x, int y);

	std::map<uint8_t, std::string> last_player_name_;
	void SVC_UpdateUserInfo();
public:
	explicit HudDeathNotice(nitroapi::NitroApiInterface* nitro_api);

	void Init() override;
	void VidInit() override;
	void Draw(float flTime) override;

	void PushDeathNotice(notice_row_t&& notice);
	inline float GetNoticeDisplayTime() { return cvar_deathnotice_time_->value; }
	inline int GetSkullSpriteIndex() { return skull_sprite_index_; }
	inline int GetDrawStringFontHeight() { return draw_string_font_height_; }

	void SetWpnIconForNextMessage(wpn_icon_override_t&& wpn_icon);

	bool HandleAmxxKillAssistCaseIfSo(int killer_id, int& assistant_id, notice_row_t* notice);
};