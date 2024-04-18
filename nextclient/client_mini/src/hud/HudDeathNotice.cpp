#include "HudDeathNotice.h"
#include "../main.h"
#include "../utils.h"
#include <parsemsg.h>
#include "triangleapi.h"

constexpr static auto KILL_RARITY_SPRITE = "sprites/kill_rarity.spr";
constexpr static int DEATHNOTICE_TOP = 32;
constexpr static int DEATHNOTICE_RIGHT = 16;

static int MsgFunc_DeathMsg(const char* pszName, int iSize, void* pbuf) {
	BEGIN_READ(pbuf, iSize);

	const int killer_id = READ_BYTE();
	const int victim_id = READ_BYTE();
	const bool is_headshot = READ_BYTE();
	std::string killed_with = READ_STRING();

	int assistant_id = 0;
	int kill_rarity_flags = 0;

	if(is_headshot)
		kill_rarity_flags |= KILLRARITY_HEADSHOT;
	
	auto extra_flags = (DeathMessageFlags)READ_LONG();
	if(READ_OK()) {
		if(extra_flags & PLAYERDEATH_POSITION) {
			READ_COORD();
			READ_COORD();
			READ_COORD();
		}

		if(extra_flags & PLAYERDEATH_ASSISTANT)
			assistant_id = READ_BYTE();

		if(extra_flags & PLAYERDEATH_KILLRARITY)
			kill_rarity_flags = READ_LONG();
	}

	auto hud = g_GameHud->get_deathnotice();
	HudDeathNotice::notice_row_t notice{};

	hud->HandleAmxxKillAssistCaseIfSo(killer_id, assistant_id, &notice);

	if(hud->IsValidClientIndex(killer_id)) {
		hud_player_info_t killer_info;
		gEngfuncs.pfnGetPlayerInfo(killer_id, &killer_info);

		if(notice.killer_name.empty() && killer_info.name != nullptr)
			notice.killer_name = killer_info.name;

		notice.killer_color = hud->GetClientColor(killer_id);

		if(killer_info.thisplayer) 
			notice.is_should_kill_highlight = true;
	}

	bool is_local_player_victim = false;

	if(hud->IsValidClientIndex(victim_id)) {
		hud_player_info_t victim_info;
		gEngfuncs.pfnGetPlayerInfo(victim_id, &victim_info);

		if(victim_info.name != nullptr)
			notice.victim_name = victim_info.name;
		
		notice.victim_color = hud->GetClientColor(victim_id);

		if(victim_info.thisplayer) {
			notice.is_should_dead_highlight = true;
			is_local_player_victim = true;
		}
	}

	if(hud->IsValidClientIndex(assistant_id)) {
		hud_player_info_t assistant_info;
		gEngfuncs.pfnGetPlayerInfo(assistant_id, &assistant_info);

		if(assistant_info.name != nullptr)
			notice.assistant_name = assistant_info.name;

		notice.assistant_color = hud->GetClientColor(assistant_id);

		if(assistant_info.thisplayer) 
			notice.is_should_kill_highlight = true;
	}

	notice.killer_id = killer_id;
	notice.is_teamkill = killed_with == "teammate";
	notice.display_time = *gHUD->m_flTime + hud->GetNoticeDisplayTime();

	if(kill_rarity_flags & KILLRARITY_DOMINATION) {
		if(is_local_player_victim && ~kill_rarity_flags & KILLRARITY_DOMINATION_BEGAN)
			kill_rarity_flags &= ~KILLRARITY_DOMINATION;
	}
	notice.kill_rarity_flags = (KillRarity)kill_rarity_flags;

	killed_with = "d_" + killed_with;
	auto spriteIndex = gHUD->GetSpriteIndex(killed_with.c_str());
	notice.weapon_sprite_index = spriteIndex != -1 ? spriteIndex : hud->GetSkullSpriteIndex();

	hud->PushDeathNotice(std::move(notice));

	return 0;
}

static int MsgFunc_DeathMsgWpnIcon(const char* pszName, int iSize, void* pbuf) {
	BEGIN_READ(pbuf, iSize);

	auto hud = g_GameHud->get_deathnotice();

	const char* sprite_path = READ_STRING();
	const int frame = READ_BYTE();
	const int rendermode = READ_BYTE();
	const int r = READ_BYTE();
	const int g = READ_BYTE();
	const int b = READ_BYTE();
	const int a = READ_BYTE();

    if (sprite_path == nullptr || !sprite_path[0] || !IsSafeSpriteFilePath(sprite_path)) {
        gEngfuncs.Con_DPrintf("MsgFunc_DeathMsgWpnIcon: invalid spritePath\n");
        return 1;
    }

	HudDeathNotice::wpn_icon_override_t wpn_icon{};
	wpn_icon.sprite = gEngfuncs.pfnSPR_Load(sprite_path);
	wpn_icon.frame = frame;
	wpn_icon.rendermode = rendermode;
	wpn_icon.color[0] = r / 255.0f;
	wpn_icon.color[1] = g / 255.0f;
	wpn_icon.color[2] = b / 255.0f;
	wpn_icon.alpha = a / 255.0f;

	float spr_w = gEngfuncs.pfnSPR_Width(wpn_icon.sprite, wpn_icon.frame);
	float spr_h = gEngfuncs.pfnSPR_Height(wpn_icon.sprite, wpn_icon.frame); 

	wpn_icon.ideal_scale = hud->GetDrawStringFontHeight() / spr_h;
	wpn_icon.ideal_w = std::ceil(spr_w * wpn_icon.ideal_scale);
	wpn_icon.ideal_h = std::ceil(spr_h * wpn_icon.ideal_scale);

	hud->SetWpnIconForNextMessage(std::move(wpn_icon));

	return 1;
}

void HudDeathNotice::SVC_UpdateUserInfo() {
	int id = eng()->MSG_ReadByte();
	int userId = eng()->MSG_ReadLong();

	auto userinfo = eng()->MSG_ReadString();
	auto current_name = client_state()->players[id].name;
	auto incoming_name = pmove->PM_Info_ValueForKey(userinfo, "name");

	if(current_name[0] && std::string(current_name) != incoming_name)
		last_player_name_[id + 1] = current_name;
}

float calculateMatchingPercentage(const std::string& old_name, const std::string& new_name, size_t& mismatchPosition) {
    size_t m = 0;
    mismatchPosition = std::string::npos;

    for (m = 0; m < old_name.size(); m++) {
		if(m >= new_name.size() || old_name[m] != new_name[m])
			break;
    }

	while(m < new_name.size() && new_name[m] == '.')
		m++;

	mismatchPosition = m;

    return (static_cast<float>(m) / old_name.size()) * 100.0;
}

bool HudDeathNotice::HandleAmxxKillAssistCaseIfSo(int killer_id, int& assistant_id, HudDeathNotice::notice_row_t* notice) {
	if(!last_player_name_.contains(killer_id)) return false;

	std::string old_name = last_player_name_[killer_id];

	if(assistant_id != 0) {
		notice->killer_name = old_name;
		return true;
	}

	hud_player_info_t player_info;
	cl_enginefunc()->pfnGetPlayerInfo(killer_id, &player_info);
	if(player_info.name == nullptr) return false;
	
	std::string new_name = player_info.name;

	constexpr const char* delim = " + ";
	constexpr size_t delim_len = std::string_view(delim).size();
	constexpr size_t min_name_len = delim_len + 2;

	if(new_name.length() < min_name_len || old_name.length() >= new_name.length()) return false;
	if(!new_name.contains(delim)) return false;

	size_t mismatch_pos;
	if(calculateMatchingPercentage(old_name, new_name, mismatch_pos) < 25.0) return false;

	size_t actual_delim_pos = mismatch_pos == std::string::npos ? old_name.length() : mismatch_pos;
	if(new_name.compare(actual_delim_pos, delim_len, delim, delim_len) != 0) return false;

	std::string dirty_assistant_name = new_name.substr(actual_delim_pos + delim_len);
	size_t first_dot_pos = dirty_assistant_name.find_last_not_of(".");
	if(first_dot_pos != std::string::npos && first_dot_pos != dirty_assistant_name.length() - 1)
		dirty_assistant_name.erase(first_dot_pos + 1);
 
	for(int i = 1; i < MAX_PLAYERS; i++) {
		hud_player_info_t player_info;
		cl_enginefunc()->pfnGetPlayerInfo(i, &player_info);

		if(player_info.name && std::string(player_info.name).starts_with(dirty_assistant_name)) {
			notice->killer_name = old_name;
			assistant_id = i;
			return true;
		}
	}
	return false;
}

HudDeathNotice::HudDeathNotice(nitroapi::NitroApiInterface* nitro_api)
	: HudBaseHelper(nitro_api) {

	DeferUnsub(cl()->CHudDeathNotice__Draw |= [this](CHudDeathNotice* const ptr, float flTime, const auto& next) {
		return cvar_deathnotice_old_->value ? next->Invoke(ptr, flTime) : 1;
	});

	DeferUnsub(cl()->UserMsg_DeathMsg += [this](const char* pszName, int iSize, void* pbuf_) {
		MsgFunc_DeathMsg(pszName, iSize, pbuf_);
	});

	DeferUnsub(eng()->SVC_UpdateUserInfo |= [this](const auto& next) {
		auto readcount = *eng()->msg_readcount;
		SVC_UpdateUserInfo();
		*eng()->msg_readcount = readcount;

		next->Invoke();
	});
}

void HudDeathNotice::PushDeathNotice(notice_row_t&& notice) {
	notice.custom_weapon_sprite = next_custom_weapon_sprite_;
	next_custom_weapon_sprite_ = {};

	notice_rows_.push_back(notice);
	if(notice_rows_.size() > cvar_deathnotice_max_->value) 
		notice_rows_.erase(notice_rows_.begin());
}

void HudDeathNotice::SetWpnIconForNextMessage(wpn_icon_override_t&& wpn_icon) {
	next_custom_weapon_sprite_ = wpn_icon;
}

void HudDeathNotice::Init() {
	cvar_deathnotice_time_ = cl_enginefunc()->pfnGetCvarPointer("hud_deathnotice_time");
	cvar_deathnotice_max_ = cl_enginefunc()->pfnRegisterVariable("hud_deathnotice_max", "5", FCVAR_ARCHIVE);
	cvar_deathnotice_old_ = cl_enginefunc()->pfnRegisterVariable("hud_deathnotice_old", "0", FCVAR_ARCHIVE);

	cl_enginefunc()->pfnHookUserMsg("DeathMsgWpn", MsgFunc_DeathMsgWpnIcon);
}

void HudDeathNotice::VidInit() {
	kill_rarity_sprite_ = LoadSprite(KILL_RARITY_SPRITE);

	skull_sprite_index_ = gHUD()->GetSpriteIndex("d_skull");
	draw_string_font_height_ = DrawConsoleStringHeight();

	kill_rarity_sprite_scale_ = 0.375;
	kill_rarity_sprite_width_ = SPR_Width(kill_rarity_sprite_, 0) * kill_rarity_sprite_scale_;
	kill_rarity_sprite_height_ = SPR_Height(kill_rarity_sprite_, 0) * kill_rarity_sprite_scale_;
	kill_rarity_sprite_alpha_ = 0.68;
	kill_rarity_sprite_rendermode_ = kRenderTransAdd;
	kill_rarity_sprite_padding_x_ = 3;

	weapon_sprite_padding_x_ = 3;
	string_padding_x_ = 3;

	notice_boxes_gap_ = 4;
	notice_box_padding_top_ = 3;
	notice_box_padding_bottom_ = 3;
	notice_box_outline_width_ = 1;
	notice_box_padding_x_ = 8;
	notice_box_height_ = draw_string_font_height_ + notice_box_padding_top_ + notice_box_padding_bottom_;

	notice_rows_.clear();
}

int HudDeathNotice::DrawScaledSprite(
	HSPRITE_t* sprite, int frame,
	int x, int y, float scale,
	int rendermode, vec3_t color, float alpha
) {
    const auto sprite_ptr = gEngfuncs.GetSpritePointer(*sprite);
	if(sprite_ptr == nullptr) return x;
	
    const auto tri = gEngfuncs.pTriAPI;

	tri->SpriteTexture(const_cast<model_s*>(sprite_ptr), frame);
    tri->RenderMode(rendermode);
    tri->Color4f(color[0], color[1], color[2], alpha);
    tri->CullFace(TRI_NONE);

	int w = gEngfuncs.pfnSPR_Width(*sprite, frame) * scale;
	int h = gEngfuncs.pfnSPR_Height(*sprite, frame) * scale;

	tri->Begin(TRI_QUADS);
	tri->TexCoord2f(0, 1);
	tri->Vertex3f(x, y + h, 0);
	tri->TexCoord2f(1, 1);
	tri->Vertex3f(x + w, y + h, 0);
	tri->TexCoord2f(1, 0);
	tri->Vertex3f(x + w, y, 0);
	tri->TexCoord2f(0, 0);
	tri->Vertex3f(x, y, 0);
	tri->End();

	tri->RenderMode(kRenderNormal);

	return x + w;
}

int HudDeathNotice::DrawKillRaritySprite(RarityFrame type, int x, int y) {
	return DrawScaledSprite(
		&kill_rarity_sprite_, type,
		x + kill_rarity_sprite_padding_x_, y, kill_rarity_sprite_scale_, 
		kill_rarity_sprite_rendermode_, sprite_icons_color_, kill_rarity_sprite_alpha_
	) + kill_rarity_sprite_padding_x_;
}

int HudDeathNotice::GetKillRaritySpriteFullWidth() {
	return kill_rarity_sprite_width_ + (kill_rarity_sprite_padding_x_ * 2);
}

int HudDeathNotice::DrawWeaponSprite(int index, int x, int y) {
	SPR_Set(gHUD()->GetSprite(index), 255, 255, 255);
	x += weapon_sprite_padding_x_;
	SPR_DrawAdditive(0, x, y, &gHUD()->GetSpriteRect(index));
	return x + gHUD()->GetSpriteWidth(index) + weapon_sprite_padding_x_;
}

int HudDeathNotice::GetWeaponSpriteFullWidth(int index) {
	return gHUD()->GetSpriteWidth(index) + weapon_sprite_padding_x_ * 2;
}

int HudDeathNotice::DrawString(const char* text, vec3_t color, int x, int y) {
	DrawSetTextColor(color);
	return DrawConsoleString(text, x + string_padding_x_, y) + string_padding_x_;
}

int HudDeathNotice::GetStringFullWidth(const char* text) {
	return DrawConsoleStringLen(text) + string_padding_x_ * 2;
}

int HudDeathNotice::GetCustomWeaponSpriteFullWidth(wpn_icon_override_t* icon) {
	return icon->ideal_w + weapon_sprite_padding_x_ * 2;
}

int HudDeathNotice::GetCustomWeaponSpriteHeight(wpn_icon_override_t* icon) {
	return icon->ideal_h;
}

int HudDeathNotice::DrawCustomWeaponSprite(wpn_icon_override_t* icon, int x, int y) {
	return DrawScaledSprite(
		&icon->sprite, icon->frame,
		x + weapon_sprite_padding_x_, y, icon->ideal_scale, 
		icon->rendermode, icon->color, icon->alpha
	) + weapon_sprite_padding_x_;
}

void HudDeathNotice::Draw(float flTime) {
	if(cvar_deathnotice_old_->value) return;

	int x, y, i = 0;
	int screen_w, screen_h;
	GetScreenResolution(screen_w, screen_h);

	for(auto notice = notice_rows_.begin(); notice != notice_rows_.end(); ) {
		if(notice->display_time < flTime) {
			notice = notice_rows_.erase(notice);
			continue;
		}

		notice->display_time = std::min(notice->display_time, m_flTime + cvar_deathnotice_time_->value);

		int weapon_sprite_full_w, weapon_sprite_h;
		if(notice->custom_weapon_sprite.sprite) {
			weapon_sprite_full_w = GetCustomWeaponSpriteFullWidth(&notice->custom_weapon_sprite);
			weapon_sprite_h = GetCustomWeaponSpriteHeight(&notice->custom_weapon_sprite);
		}
		else {
			weapon_sprite_full_w = GetWeaponSpriteFullWidth(notice->weapon_sprite_index);
			weapon_sprite_h = gHUD()->GetSpriteHeight(notice->weapon_sprite_index);
		}

		y = (DEATHNOTICE_TOP * (screen_h / 480.0f) + 0.5f) 
			+ ((notice_box_height_ + notice_box_outline_width_ * 2 + notice_boxes_gap_) * i);
		x = screen_w - DEATHNOTICE_RIGHT - weapon_sprite_full_w;

		if(g_iUser1 != 0)
			y += 80;

		int weapon_sprite_optimal_y = y + ((notice_box_height_ - weapon_sprite_h) / 2);
		int kill_rarity_sprite_optimal_y = y + ((notice_box_height_ - kill_rarity_sprite_height_) / 2);
		int draw_string_optimal_y = y + notice_box_padding_top_;
		
		if(notice->killer_name.length())
			x -= GetStringFullWidth(notice->killer_name.c_str());

		if(notice->victim_name.length())
			x -= GetStringFullWidth(notice->victim_name.c_str());

		if(notice->assistant_name.length())
			x -= GetStringFullWidth(notice->assistant_name.c_str()) + GetStringFullWidth("+");
		
		for(int flag = KILLRARITY_HEADSHOT; flag <= KILLRARITY_ASSISTEDFLASH; flag <<= 1) {
			if(notice->kill_rarity_flags & flag)
				x -= GetKillRaritySpriteFullWidth();
		}

		if(notice->kill_rarity_flags & (KILLRARITY_DOMINATION|KILLRARITY_REVENGE))
			x -= GetKillRaritySpriteFullWidth();

		if(notice->is_should_dead_highlight) {
			DrawRect(
				x - notice_box_padding_x_, y, 
				screen_w - DEATHNOTICE_RIGHT + notice_box_padding_x_, y + notice_box_height_,
				150, 0, 20, 100
			);
		}
		else if(notice->is_should_kill_highlight) {
			DrawOutlinedRect(
				x - notice_box_padding_x_, y, 
				screen_w - DEATHNOTICE_RIGHT + notice_box_padding_x_, y + notice_box_height_, 
				0, 0, 0, 100, 
				notice_box_outline_width_, 230, 20, 0, 255
			);
		}
		else {
			DrawRect(
				x - notice_box_padding_x_, y, 
				screen_w - DEATHNOTICE_RIGHT + notice_box_padding_x_, y + notice_box_height_,
				0, 0, 0, 100
			);
		}

		if(notice->kill_rarity_flags & KILLRARITY_DOMINATION)
			x = DrawKillRaritySprite(RarityFrame::DOMINATION, x, kill_rarity_sprite_optimal_y);
		else if(notice->kill_rarity_flags & KILLRARITY_REVENGE)
			x = DrawKillRaritySprite(RarityFrame::REVENGE, x, kill_rarity_sprite_optimal_y);

		if(notice->kill_rarity_flags & KILLRARITY_KILLER_BLIND)
			x = DrawKillRaritySprite(RarityFrame::KILLER_BLIND, x, kill_rarity_sprite_optimal_y);

		if(notice->killer_name.length())
			x = DrawString(notice->killer_name.c_str(), notice->killer_color, x, draw_string_optimal_y);

		if(notice->assistant_name.length()) {
			x = DrawString("+", sprite_icons_color_, x, draw_string_optimal_y);

			if(notice->kill_rarity_flags & KILLRARITY_ASSISTEDFLASH)
				x = DrawKillRaritySprite(RarityFrame::ASSIST_FLASH, x, kill_rarity_sprite_optimal_y);

			x = DrawString(notice->assistant_name.c_str(), notice->assistant_color, x, draw_string_optimal_y);
		}
		
		if(notice->custom_weapon_sprite.sprite)
			x = DrawCustomWeaponSprite(&notice->custom_weapon_sprite, x, weapon_sprite_optimal_y);
		else
			x = DrawWeaponSprite(notice->weapon_sprite_index, x, weapon_sprite_optimal_y);

		if(notice->kill_rarity_flags & KILLRARITY_NOSCOPE)
			x = DrawKillRaritySprite(RarityFrame::NOSCOPE, x, kill_rarity_sprite_optimal_y);

		if(notice->kill_rarity_flags & KILLRARITY_THRUSMOKE)
			x = DrawKillRaritySprite(RarityFrame::THROUGH_SMOKE, x, kill_rarity_sprite_optimal_y);

		if(notice->kill_rarity_flags & KILLRARITY_PENETRATED)
			x = DrawKillRaritySprite(RarityFrame::PENETRATED, x, kill_rarity_sprite_optimal_y);

		if(notice->kill_rarity_flags & KILLRARITY_HEADSHOT)
			x = DrawKillRaritySprite(RarityFrame::HEADSHOT, x, kill_rarity_sprite_optimal_y);

		if(notice->victim_name.length())
			x = DrawString(notice->victim_name.c_str(), notice->victim_color, x, draw_string_optimal_y);

		i++;
		notice++;
	}
}
