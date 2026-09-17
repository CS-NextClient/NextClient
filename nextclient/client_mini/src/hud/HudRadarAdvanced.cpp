#include "HudRadarAdvanced.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>

#include "main.h"

#include <goldsrc/client_api.h>
#include <goldsrc/cstrike/game.h>
#include <goldsrc/cstrike/hud.h>
#include <goldsrc/cstrike/hud_elements.h>
#include <goldsrc/cvar.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/entity.h>
#include <goldsrc/pmove.h>
#include <goldsrc/view.h>
#include <ncl_math/vec3.h>
#include <ncl_math/color.h>
#include <next_client_mini/radar_cvars.h>

#include "radar/Overview.h"
#include "radar/RadarConfig.h"
#include "radar/RadarLimits.h"
#include "radar/RadarDraw.h"
#include "radar/RadarText.h"
#include "radar/SmokeRegistry.h"
#include "radar/RadarHooks.h"
#include "radar/HudTextZone.h"
#include "radar/RadarFade.h"
#include "radar/RadarDebug.h"
#include "radar/RadarBroadcast.h"
#include "view.h"

namespace
{
    constexpr RadarCvarDef kScaleCvar = RadarCvarDef_Get("hud_radar_scale");
    constexpr RadarCvarDef kMapScaleCvar = RadarCvarDef_Get("hud_radar_map_scale");
    static_assert(kScaleCvar.name && kMapScaleCvar.name);

    static_assert(static_cast<int>(RadarTeam::Unassigned) == goldsrc::cstrike::TEAM_UNASSIGNED);
    static_assert(static_cast<int>(RadarTeam::Terrorist) == goldsrc::cstrike::TEAM_TERRORIST);
    static_assert(static_cast<int>(RadarTeam::CounterTerrorist) == goldsrc::cstrike::TEAM_CT);
    static_assert(static_cast<int>(RadarTeam::Spectator) == goldsrc::cstrike::TEAM_SPECTATOR);

    constexpr float kRunSpeedFallback = 320.0f;

    // Set by Draw, read later in the same frame by the classic radar.
    bool g_MapUnavailable;

    void LogPolicyMessage(const char* message)
    {
        gEngfuncs.Con_Printf("radar: %s\n", message);
    }

    // Optional server-set mode hint, broadcast as "serverinfo nc_ffa 1".
    const char* GetServerInfoValue(const char* key)
    {
        const char* value = gEngfuncs.ServerInfo_ValueForKey(key);
        return value ? value : "";
    }

    // Values: "marker" re-purposes the message, the stock name ("bomb"/"hostage") pins it.
    MessageHint ParseMessageHint(const char* value, const char* stock_name)
    {
        if (stricmp(value, "marker") == 0)
        {
            return MessageHint::Marker;
        }

        if (stricmp(value, stock_name) == 0)
        {
            return MessageHint::Stock;
        }

        return MessageHint::None;
    }

} // namespace

HudRadarAdvanced::HudRadarAdvanced(nitroapi::NitroApiInterface* nitro_api) :
    HudBaseHelper(nitro_api)
{
    policy_.SetLogger(LogPolicyMessage);

    RadarSinks sinks{};
    sinks.smoke = &smoke_;
    sinks.broadcast = &broadcast_;
    RadarHooks_SetSinks(sinks);
}

HudRadarAdvanced::~HudRadarAdvanced()
{
    RadarHooks_ClearSinks();
}

void HudRadarAdvanced::OnPlayerRespawn()
{
    players_.OnPlayerRespawn();
    objectives_.OnPlayerRespawn();
}

void HudRadarAdvanced::VidInit()
{
    RadarDraw_ResetGl();
    text_.ResetGl();
    backdrop_.OnVidInit();
    players_.OnVidInit();
    objectives_.OnVidInit();
    RadarHooks_InvalidateGlyphMetrics();
    smoke_.Reset();
    broadcast_.Reset(gEngfuncs.GetClientTime());
    visibility_.Reset();
    policy_.OnLevelChange();
    last_hint_read_ = 0.0f;
    OnPlayerRespawn();
}

void HudRadarAdvanced::InitHUDData()
{
    OnPlayerRespawn(); // first spawn only (gmsgInitHUD)
}

void HudRadarAdvanced::Reset()
{
    OnPlayerRespawn(); // gmsgResetHUD: also fires on a mid-round respawn
}

RadarBox HudRadarAdvanced::ComputeBox(bool spectating)
{
    int screen_w = 0;
    int screen_h = 0;
    GetScreenResolution(screen_w, screen_h);

    float scale = std::clamp(g_RadarCfg.scale->value, kScaleCvar.min_value, kScaleCvar.max_value);

    return RadarView_ComputeBox(static_cast<float>(screen_w), static_cast<float>(screen_h), scale, spectating);
}

RadarTransform HudRadarAdvanced::ComputeTransform(const RadarBox& box, const RadarShape& shape)
{
    const RadarOverview& overview = backdrop_.get_overview();
    const overview_t& ov = overview.get_data();

    RadarViewParams p;
    p.ov_zoom = ov.zoom;
    p.ov_origin = {ov.origin[0], ov.origin[1]};
    p.ov_rotated = ov.rotated != 0;
    p.map_scale = std::clamp(g_RadarCfg.map_scale->value, kMapScaleCvar.min_value, kMapScaleCvar.max_value);
    p.rotate = g_RadarCfg.rotate->value != 0.0f;
    p.fullmap = g_RadarCfg.fullmap->value != 0.0f;
    p.round = shape.round;
    p.view_origin = {g_MainViewOrigin[0], g_MainViewOrigin[1]};
    p.view_yaw = g_MainViewAngles[1];
    p.sprite_frames = overview.get_sprite() ? overview.get_sprite()->numframes : 0;

    return RadarView_ComputeTransform(box, p);
}

bool HudRadarAdvanced::ShouldDraw()
{
    // Before the observer short-circuit below: 'hideradar' is a standing instruction, not
    // per-life state, so observing must not bring the radar back after it was turned off.
    if (!gHUD()->m_Health->m_bDrawRadar)
    {
        return false;
    }

    bool spectating = g_iUser1 != 0 || cl_enginefunc()->IsSpectateOnly();

    if (spectating)
    {
        return true;
    }

    if (m_iHideHUDDisplay & goldsrc::cstrike::HIDEHUD_HEALTH)
    {
        return false;
    }

    // The suit bit is set on every spawn, mid-round CSDM respawns included. Not m_fPlayerDead:
    // the stock client freezes it while observing in first person and clears it only on the
    // next CurWeapon, so it can stay stale-dead after a respawn and wrongly hide the radar.
    if (!(m_iWeaponBits & (1 << goldsrc::cstrike::WEAPON_SUIT)))
    {
        return false;
    }

    // On mp_fadetoblack 1 the just-killed player stays on the corpse cam with g_iUser1 still
    // 0, so without this gate the radar paints live positions over the black fade.
    if (gHUD()->m_Health->m_iHealth <= 0)
    {
        return false;
    }

    return true;
}

void HudRadarAdvanced::UpdateFrameContext(bool spectating, float flTime)
{
    ctx_ = FrameContext{};
    ctx_.spectating = spectating;

    if (goldsrc::cl_entity_t* local = cl_enginefunc()->GetLocalPlayer())
    {
        ctx_.local = local;
        ctx_.self_idx = local->index;
        ctx_.highlight_idx = local->index;
        
        if (ctx_.spectating && IsValidClientIndex(g_iUser2))
        {
            ctx_.highlight_idx = g_iUser2;
        }

        if (IsValidClientIndex(ctx_.highlight_idx))
        {
            ctx_.highlight_team = RadarTeam_From(cl()->g_PlayerExtraInfo[ctx_.highlight_idx].teamnumber);
        }
        
        ctx_.local_team = RadarTeam_From(cl()->g_PlayerExtraInfo[ctx_.self_idx].teamnumber);
    }

    // Runs here: the visibility pass below builds its sets from the policy's teammate answer.
    UpdatePolicy(flTime);

    if (ctx_.local)
    {
        visibility_.Update(ctx_.self_idx, ctx_.highlight_team, ctx_.spectating, flTime, policy_);
    }
}

void HudRadarAdvanced::UpdatePolicy(float flTime)
{
    policy_.SetLogging(RadarDebug_IsEnabled());
    policy_.SetForceAllyGating(g_RadarCfg.allies->value == 0.0f);

    const RadarMapObjectives& map = objectives_.get_map_objectives();
    policy_.SetMapObjectives(map.get_site_count(), map.get_hostage_count());

    if (flTime - last_hint_read_ >= kHintInterval || flTime < last_hint_read_)
    {
        last_hint_read_ = flTime;
        policy_.SetServerFfaHint(std::atoi(GetServerInfoValue("nc_ffa")) != 0);
        policy_.SetBombHint(ParseMessageHint(GetServerInfoValue("nc_bombmsg"), "bomb"));
        policy_.SetHostageHint(ParseMessageHint(GetServerInfoValue("nc_hostagemsg"), "hostage"));
    }

    float run_speed = (pmove && pmove->movevars) ? pmove->movevars->maxspeed : kRunSpeedFallback;
    bool applicable = ctx_.local != nullptr && !ctx_.spectating && ctx_.local_team != RadarTeam::Unassigned;
    policy_.BeginFrame(flTime, run_speed, applicable);

    // Classified for spectators too: what a message means does not depend on who watches.
    const RadarBroadcast::Bomb& bomb = broadcast_.get_bomb();
    policy_.ObserveBomb({bomb.seq, bomb.active, bomb.pos, bomb.planted, bomb.time}, flTime);

    RadarBroadcast::Hostage hostages[kRadarMaxHostages];
    int hostage_count = broadcast_.CollectHostages(hostages);

    // HostagePos reaches living Counter-Terrorists only: nobody else may read its absence.
    policy_.ObserveHostageBroadcast(!ctx_.spectating && ctx_.local_team == RadarTeam::CounterTerrorist, hostage_count);

    for (int i = 0; i < hostage_count; i++)
    {
        policy_.ObserveHostage({hostages[i].index, hostages[i].seq, hostages[i].time}, flTime);
    }

    if (!applicable)
    {
        return;
    }

    int message_num = ctx_.local->curstate.messagenum;
    int max_clients = std::min<int>(goldsrc::cstrike::MAX_PLAYERS, cl_enginefunc()->GetMaxClients());

    for (int i = 1; i <= max_clients; i++)
    {
        if (i == ctx_.self_idx)
        {
            continue;
        }

        goldsrc::cstrike::extra_player_info_t& ext = cl()->g_PlayerExtraInfo[i];

        if (ext.dead || RadarTeam_From(ext.teamnumber) != ctx_.local_team)
        {
            continue;
        }

        goldsrc::cl_entity_t* ent = cl_enginefunc()->GetEntityByIndex(i);
        bool live = ent && ent->player && ent->curstate.messagenum == message_num;
        ncl_math::Vector2 live_pos = live ? ncl_math::Vector2{ent->origin[0], ent->origin[1]} : ncl_math::Vector2{};

        policy_.ObserveAlly(i, live, live_pos, ncl_math::Vector3::Ref(ext.origin));
    }
}

void HudRadarAdvanced::Draw(float flTime)
{
    g_MapUnavailable = false;

    if (!RadarConfig_CheckAdvancedActive())
    {
        return;
    }

    if (!ShouldDraw())
    {
        return;
    }

    bool debug = RadarDebug_IsEnabled();
    std::chrono::steady_clock::time_point debug_start;

    RadarDraw_ResetStats();
    text_.ResetStats();
    visibility_.ResetStats();

    if (debug)
    {
        debug_start = std::chrono::steady_clock::now();
    }

    GetAllPlayersInfo();
    players_.BeginFrame(flTime);

    if (!backdrop_.EnsureMap())
    {
        g_MapUnavailable = true;
        return;
    }
    
    bool spectating = g_iUser1 != 0 || cl_enginefunc()->IsSpectateOnly();
    
    RadarBox box = ComputeBox(spectating);
    RadarShape shape = RadarView_ComputeShape(box, g_RadarCfg.round->value != 0.0f);
    HudTextZone_SetZone(box.x, box.y, box.size);
    RadarTransform transform = ComputeTransform(box, shape);

    UpdateFrameContext(spectating, flTime);

    // Every primitive is tinted toward the fade colour as it is drawn rather than covered
    // by an overlay fill, which would leave a fading halo around the radar.
    ncl_math::Color flash_col;
    flash_alpha_ = ComputeFlashAlpha(flash_col);

    if (flash_alpha_ > 0)
    {
        RadarDraw_SetFade(flash_col.WithAlpha(flash_alpha_));
    }
    else
    {
        RadarDraw_ClearFade();
    }

    RadarFrame frame{box, transform, flTime, (float)m_flTimeDelta, ctx_, flash_alpha_, shape};

    backdrop_.Draw(frame);
    objectives_.CollectEntities(frame);
    // Player-prediction trace context, live for the objective line-of-sight tests below.
    visibility_.BeginObjectiveTests();
    objectives_.Draw(frame);
    visibility_.EndObjectiveTests();
    players_.Draw(frame);

    RadarDraw_EndFrame();

    if (debug)
    {
        RadarDrawStats stats = RadarDraw_GetStats();
        float total_ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - debug_start).count();
        RadarDebug_DrawFrame(box, total_ms, stats.blur_ms, visibility_.get_traces_ms(), text_.get_ms(), stats.batches);
    }
}

int HudRadarAdvanced::ComputeFlashAlpha(ncl_math::Color& color)
{
    // The engine blends the screen fade onto the 3D scene before the HUD is drawn, so a
    // flashbang leaves the radar fully readable unless it is washed here too.
    goldsrc::screenfade_t fade;
    std::memset(&fade, 0, sizeof(fade));
    cl_enginefunc()->pfnGetScreenFade(&fade);

    // Near-white only, so death and round-transition fades do not wash the radar. Shared
    // with the enemy line-of-sight gating so the two agree on what counts as a flash.
    if (!RadarFade_IsNearWhite(fade))
    {
        return 0;
    }

    int a = RadarFade_ComputeAlpha(fade, cl_enginefunc()->GetClientTime());

    if (a <= 0)
    {
        return 0;
    }

    color.r = fade.fader;
    color.g = fade.fadeg;
    color.b = fade.fadeb;

    return a;
}

bool HudRadarAdvanced::is_map_unavailable()
{
    return g_MapUnavailable;
}
