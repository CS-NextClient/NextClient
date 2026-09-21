#include "HudDeathNotice.h"
#include "../main.h"
#include "../utils.h"
#include <parsemsg.h>
#include "triangleapi.h"

#include <iterator>
#include <string_view>

constexpr static auto kKillRaritySprite = "sprites/kill_rarity.spr";
constexpr static int kDeathNoticeTop = 32;
constexpr static int kDeathNoticeRight = 16;

constexpr static int kSpectatorTopBarHeight = 64;

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

	hud->ApplyKillAssistRename(killer_id, assistant_id, &notice);

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

namespace {
	constexpr std::string_view kKillAssistDelim = " + ";
	constexpr float kKillAssistMinKillerNamePercent = 25.0f;

	// True when combined is "<original> + <assistant>" as built by the AMXX Kill Assist plugin,
	// which trims either part down to the engine name limit and marks the cut with trailing dots.
	// assistant_name receives the trimmed assistant part, so it is a prefix of the real nickname.
	bool SplitKillAssistName(std::string_view original, std::string_view combined, std::string_view& assistant_name) {
		if(original.empty()) return false;

		size_t matched = 0;
		while(matched < original.size() && matched < combined.size() && original[matched] == combined[matched])
			matched++;

		if(static_cast<float>(matched) / original.size() * 100.0f < kKillAssistMinKillerNamePercent)
			return false;

		size_t delim_pos = matched;
		while(delim_pos < combined.size() && combined[delim_pos] == '.')
			delim_pos++;

		if(!combined.substr(delim_pos).starts_with(kKillAssistDelim)) return false;

		assistant_name = combined.substr(delim_pos + kKillAssistDelim.size());

		size_t last_kept = assistant_name.find_last_not_of('.');
		if(last_kept == std::string_view::npos) return false;

		assistant_name = assistant_name.substr(0, last_kept + 1);

		return true;
	}
}

void HudDeathNotice::SVC_UpdateUserInfo() {
	int id = eng()->MSG_ReadByte();
	eng()->MSG_ReadLong();

	auto client = client_state();
	if(id < 0 || id >= static_cast<int>(std::size(client->players))) return;

	auto userinfo = eng()->MSG_ReadString();
	auto current_name = client->players[id].name;
	auto incoming_name = pmove->PM_Info_ValueForKey(userinfo, "name");

	if(current_name[0] && std::string_view(current_name) != incoming_name)
		previous_player_names_[id + 1] = current_name;
}

std::string_view HudDeathNotice::get_player_name(int client_index) {
	hud_player_info_t player_info;
	cl_enginefunc()->pfnGetPlayerInfo(client_index, &player_info);

	return player_info.name != nullptr ? player_info.name : "";
}

// Returns the connected player whose name starts with name_prefix, preferring an exact match,
// or 0 when nothing or an empty prefix is given.
int HudDeathNotice::FindPlayerByNamePrefix(std::string_view name_prefix, int skip_client_index) {
	if(name_prefix.empty()) return 0;

	int prefix_match = 0;

	for(int i = 1; i <= MAX_PLAYERS; i++) {
		if(i == skip_client_index) continue;

		std::string_view name = get_player_name(i);
		if(name.empty()) continue;

		if(name == name_prefix) return i;

		if(prefix_match == 0 && name.starts_with(name_prefix))
			prefix_match = i;
	}

	return prefix_match;
}

// Undoes the killer rename the AMXX Kill Assist plugin performs when the server has no native
// assist support: restores the killer name the notice should carry and, when the server did not
// send an assistant index itself, recovers it from the appended part of the name.
void HudDeathNotice::ApplyKillAssistRename(int killer_id, int& assistant_id, notice_row_t* notice) {
	if(!IsValidClientIndex(killer_id)) return;

	const std::string& previous_name = previous_player_names_[killer_id];
	if(previous_name.empty()) return;

	std::string_view assistant_name;
	if(!SplitKillAssistName(previous_name, get_player_name(killer_id), assistant_name)) return;

	if(assistant_id == 0)
		assistant_id = FindPlayerByNamePrefix(assistant_name, killer_id);
	else if(!get_player_name(assistant_id).starts_with(assistant_name))
		return;

	notice->killer_name = previous_name;

	if(!IsValidClientIndex(assistant_id)) {
		notice->assistant_name = assistant_name;
		notice->assistant_color = sprite_icons_color_;
	}
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

	DeferUnsub(eng()->CL_ParseServerMessage |= [this](qboolean normal_message, const auto& next) {
		next->Invoke(normal_message);

		for(std::string& name : previous_player_names_)
			name.clear();
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
	kill_rarity_sprite_ = LoadSprite(kKillRaritySprite);

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

	int notices_top_units = kDeathNoticeTop;

	if(g_iUser1 != 0)
		notices_top_units += kSpectatorTopBarHeight;

	const int notices_top = static_cast<int>(notices_top_units * (screen_h / 480.0f) + 0.5f);

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

		y = notices_top
			+ ((notice_box_height_ + notice_box_outline_width_ * 2 + notice_boxes_gap_) * i);
		x = screen_w - kDeathNoticeRight - weapon_sprite_full_w;

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

		if(notice->kill_rarity_flags & KILLRARITY_INAIR)
			x -= GetKillRaritySpriteFullWidth();

		if(notice->is_should_dead_highlight) {
			DrawRect(
				x - notice_box_padding_x_, y, 
				screen_w - kDeathNoticeRight + notice_box_padding_x_, y + notice_box_height_,
				150, 0, 20, 100
			);
		}
		else if(notice->is_should_kill_highlight) {
			DrawOutlinedRect(
				x - notice_box_padding_x_, y, 
				screen_w - kDeathNoticeRight + notice_box_padding_x_, y + notice_box_height_, 
				0, 0, 0, 100, 
				notice_box_outline_width_, 230, 20, 0, 255
			);
		}
		else {
			DrawRect(
				x - notice_box_padding_x_, y, 
				screen_w - kDeathNoticeRight + notice_box_padding_x_, y + notice_box_height_,
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

		if(notice->kill_rarity_flags & KILLRARITY_INAIR)
			x = DrawKillRaritySprite(RarityFrame::KILLER_INAIR, x, kill_rarity_sprite_optimal_y - (kill_rarity_sprite_height_ / 2));

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
