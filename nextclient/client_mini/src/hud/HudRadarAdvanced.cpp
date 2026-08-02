#include "HudRadarAdvanced.h"
#include "radar/RadarConfig.h"
#include "radar/RadarDraw.h"
#include "radar/RadarText.h"
#include "radar/SmokeRegistry.h"
#include "radar/RadarHooks.h"
#include "radar/HudTextZone.h"
#include "radar/RadarFade.h"
#include "radar/RadarDebug.h"
#include "radar/RadarDump.h"
#include "radar/RadarBroadcast.h"
#include "../main.h"
#include "../view.h"

#include <triangleapi.h>
#include <screenfade.h>
#include <ncl_math/vec3.h>
#include <ncl_math/color.h>
#include <ncl_math/clip.h>
#include <next_client_mini/radar_cvars.h>
#include <pm_defs.h>
#include <pm_movevars.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>

namespace
{
    // Cvar value ranges shared with the settings dialog (see radar_cvars.h).
    constexpr RadarCvarDef kScaleCvar = RadarCvarDef_Get("hud_radar_scale");
    constexpr RadarCvarDef kMapScaleCvar = RadarCvarDef_Get("hud_radar_map_scale");
    constexpr RadarCvarDef kOpacityCvar = RadarCvarDef_Get("hud_radar_opacity");
    static_assert(kScaleCvar.name && kMapScaleCvar.name && kOpacityCvar.name);

    // Fallback until the server movevars arrive; the policy scales its broadcast-lag
    // tolerance with the running speed.
    constexpr float kRunSpeedFallback = 320.0f;

    // Live hostage broadcasts pulled per frame; stock maps hold far fewer, and a mod
    // re-purposing the message sends one or two markers.
    constexpr int kMaxBroadcastHostages = 24;

    void PolicyLog(const char* message)
    {
        gEngfuncs.Con_Printf("radar: %s\n", message);
    }

    // Optional serverinfo keys a server can set once ("serverinfo nc_ffa 1") to declare
    // its mode instead of leaving the radar to work it out from the message stream.
    const char* ServerInfoValue(const char* key)
    {
        const char* value = gEngfuncs.ServerInfo_ValueForKey(key);
        return value ? value : "";
    }

    // "marker" re-purposes the message; the objective's own name ("bomb" / "hostage")
    // pins it to its stock meaning.
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

    // Radar fill/tile opacity as a 0..1 fraction, clamped to the cvar's range.
    float OpacityFraction()
    {
        return std::clamp(g_RadarCfg.opacity->value, kOpacityCvar.min_value, kOpacityCvar.max_value) / kOpacityCvar.max_value;
    }

    // Clip a map tile quad to the radar box, recomputing UVs. out holds up to 8
    // vertices. Used only on the no-shader fallback, where nothing masks the overspill.
    int ClipTileToBox(const ncl_math::UvVertex quad[4], const RadarBox& box, ncl_math::UvVertex* out)
    {
        ncl_math::UvVertex a[10], b[10];
        int n = ncl_math::ClipHalfPlane(quad, 4, 0, box.x, true, a);
        n = ncl_math::ClipHalfPlane(a, n, 0, box.x + box.size, false, b);
        n = ncl_math::ClipHalfPlane(b, n, 1, box.y, true, a);
        n = ncl_math::ClipHalfPlane(a, n, 1, box.y + box.size, false, out);
        return n;
    }

} // namespace

HudRadarAdvanced::HudRadarAdvanced(nitroapi::NitroApiInterface* nitro_api) :
    HudBaseHelper(nitro_api)
{
    policy_.SetLogger(PolicyLog);
}

void HudRadarAdvanced::ResetRound()
{
    // Wipe everything carried between rounds so trails, death marks and the fading
    // hostage/bomb markers do not bleed into the next round. The RadarBroadcast bomb
    // (server-pushed via gmsgBombDrop, not re-sent on a respawn) is the exception: it is
    // cleared on a level change (VidInit) and by gmsgBombPickup, NOT here, because
    // gmsgResetHUD also fires on mid-round respawns and clearing it would wipe a live
    // bomb until round end. The client-derived bomb display state below still resets
    // (the CT re-spots by line of sight; a genuine round restart clears RadarBroadcast).
    players_.ResetRound();
    objectives_.ResetRound();
}

void HudRadarAdvanced::VidInit()
{
    RadarDraw_ResetGl(); // a video mode change may have recreated the GL context
    RadarText_ResetGl();
    overview_.Invalidate(); // rebuild on every level (re)load; engine textures change
    clamped_sprite_ = nullptr; // sprite textures are recreated with default wrap
    players_.OnVidInit();
    objectives_.OnVidInit();
    RadarHooks_InvalidateGlyphMetrics(); // fonts rebuilt: cached glyph widths are stale
    visibility_.Reset();
    // The mode decisions are per map: bomb targets, the server's hints and every
    // teammate broadcast start over on the next level.
    policy_.OnLevelChange();
    last_hint_read_ = 0.0f;
    ResetRound();
}

void HudRadarAdvanced::InitHUDData()
{
    ResetRound(); // first spawn only (gmsgInitHUD)
}

void HudRadarAdvanced::Reset()
{
    ResetRound(); // gmsgResetHUD: fires on every round restart / respawn
}

RadarBox HudRadarAdvanced::ComputeBox()
{
    SCREENINFO screen;
    screen.iSize = sizeof(SCREENINFO);
    cl_enginefunc()->pfnGetScreenInfo(&screen);

    float scale = std::clamp(g_RadarCfg.scale->value, kScaleCvar.min_value, kScaleCvar.max_value);
    bool spectating = g_iUser1 != 0 || cl_enginefunc()->IsSpectateOnly();

    return RadarView_ComputeBox((float)screen.iWidth, (float)screen.iHeight, scale, spectating);
}

RadarTransform HudRadarAdvanced::ComputeTransform(const RadarBox& box)
{
    const overview_t& ov = overview_.data();

    RadarViewParams p;
    p.ov_zoom = ov.zoom;
    p.ov_origin = {ov.origin[0], ov.origin[1]};
    p.ov_rotated = ov.rotated != 0;
    p.map_scale = std::clamp(g_RadarCfg.map_scale->value, kMapScaleCvar.min_value, kMapScaleCvar.max_value);
    p.rotate = g_RadarCfg.rotate->value != 0.0f;
    p.fullmap = g_RadarCfg.fullmap->value != 0.0f;
    p.round = g_RadarCfg.round->value != 0.0f;
    p.view_origin = {g_MainViewOrigin[0], g_MainViewOrigin[1]};
    p.view_yaw = g_MainViewAngles[1];
    p.sprite_frames = overview_.sprite() ? overview_.sprite()->numframes : 0;

    return RadarView_ComputeTransform(box, p);
}

void HudRadarAdvanced::DrawBackground(const RadarBox& box)
{
    float opacity = OpacityFraction();
    int a = (int)(opacity * 255.0f);

    if (a <= 0)
    {
        return;
    }

    bool round = g_RadarCfg.round->value != 0.0f;

    if (g_RadarCfg.bg->value != 0.0f &&
        RadarDraw_DrawBlurBackground(box.x, box.y, box.size, !round, 0.35f, opacity, box.screen_w, box.screen_h))
    {
        return;
    }

    if (round)
    {
        RadarDraw_DrawDisc({box.x + box.size * 0.5f, box.y + box.size * 0.5f}, box.size * 0.5f, {0, 0, 0, a});
    }
    else
    {
        ncl_math::Color c = RadarDraw_FadeColor({0, 0, 0});
        FillRGBABlend((int)box.x, (int)box.y, (int)box.size, (int)box.size, c.r, c.g, c.b, a);
    }
}

// No overview sprite: a top-down BSP image was generated at level load. Project
// its world-aligned corners through the same transform as the markers and draw
// it as one masked textured quad.
void HudRadarAdvanced::DrawRuntimeMap(const RadarBox& box, const RadarTransform& t)
{
    float opacity = OpacityFraction();
    bool round = g_RadarCfg.round->value != 0.0f;

    const RuntimeMap& rm = overview_.runtime_map();
    const float uv[8] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    ncl_math::Vector2 c0 = t.WorldToScreen({rm.bounds.min.x, rm.bounds.min.y});
    ncl_math::Vector2 c1 = t.WorldToScreen({rm.bounds.max.x, rm.bounds.min.y});
    ncl_math::Vector2 c2 = t.WorldToScreen({rm.bounds.max.x, rm.bounds.max.y});
    ncl_math::Vector2 c3 = t.WorldToScreen({rm.bounds.min.x, rm.bounds.max.y});
    float xy[8] = {c0.x, c0.y, c1.x, c1.y, c2.x, c2.y, c3.x, c3.y};
    RadarDraw_DrawMapTexture(rm.texture, xy, uv, {1.0f, 1.0f, 1.0f, opacity}, t.center, box.size * 0.5f, !round);
}

void HudRadarAdvanced::DrawOverviewTiles(const RadarBox& box, const RadarTransform& t)
{
    model_t* map = overview_.sprite();

    if (!map)
    {
        return;
    }

    float opacity = OpacityFraction();
    bool round = g_RadarCfg.round->value != 0.0f;

    int total_frames = t.total_frames;
    int x_tiles = t.x_tiles;
    int y_tiles = t.y_tiles;

    triangleapi_t* tri = cl_enginefunc()->pTriAPI;
    tri->RenderMode(kRenderTransAlpha);
    tri->CullFace(TRI_NONE);
    tri->Color4f(1.0f, 1.0f, 1.0f, opacity);

    // Clamp every frame texture once per loaded sprite so GL_LINEAR does not sample
    // past tile edges (transparent seams); the wrap state then persists per texture.
    if (map != clamped_sprite_)
    {
        for (int f = 0; f < total_frames; f++)
        {
            tri->SpriteTexture(map, f);
            RadarDraw_ClampBoundTexture();
        }

        clamped_sprite_ = map;
    }

    bool masked = g_RadarCfg.map_mask->value != 0.0f && RadarDraw_Begin(t.center, box.size * 0.5f, 1.5f, 0.0f, !round);

    ncl_math::Vector2 row = t.out;

    int frame = 0;
    for (int ty = 0; ty < y_tiles; ty++)
    {
        ncl_math::Vector2 base = row;

        for (int tx = 0; tx < x_tiles; tx++)
        {
            if (frame >= total_frames)
            {
                break;
            }

            // Tiles fully outside the radar box draw nothing (the mask shader
            // discards every pixel); skip them. In zoomed follow mode that is
            // most of the grid.
            ncl_math::Vector2 far_corner = base + t.right + t.up;
            float tile_min_x = std::min({base.x, base.x + t.right.x, base.x + t.up.x, far_corner.x});
            float tile_max_x = std::max({base.x, base.x + t.right.x, base.x + t.up.x, far_corner.x});
            float tile_min_y = std::min({base.y, base.y + t.right.y, base.y + t.up.y, far_corner.y});
            float tile_max_y = std::max({base.y, base.y + t.right.y, base.y + t.up.y, far_corner.y});
            bool overlaps = tile_max_x >= box.x && tile_min_x <= box.x + box.size && tile_max_y >= box.y &&
                tile_min_y <= box.y + box.size;

            if (overlaps)
            {
                ncl_math::Vector2 right_corner = base + t.right;
                ncl_math::Vector2 up_corner = base + t.up;
                tri->SpriteTexture(map, frame);

                if (masked)
                {
                    // The mask shader discards fragments outside the box, so draw the
                    // whole tile and let it feather the radar edge.
                    tri->Begin(TRI_QUADS);
                    tri->TexCoord2f(0, 0);
                    tri->Vertex3f(base.x, base.y, 0.0f);
                    tri->TexCoord2f(0, 1);
                    tri->Vertex3f(right_corner.x, right_corner.y, 0.0f);
                    tri->TexCoord2f(1, 1);
                    tri->Vertex3f(far_corner.x, far_corner.y, 0.0f);
                    tri->TexCoord2f(1, 0);
                    tri->Vertex3f(up_corner.x, up_corner.y, 0.0f);
                    tri->End();
                }
                else
                {
                    // No mask shader: clip the tile to the box on the CPU so the map does
                    // not spill past the radar (hard, square edge in this fallback).
                    const ncl_math::UvVertex quad[4] = {
                        {base, {0.0f, 0.0f}},
                        {right_corner, {0.0f, 1.0f}},
                        {far_corner, {1.0f, 1.0f}},
                        {up_corner, {1.0f, 0.0f}},
                    };
                    ncl_math::UvVertex clipped[10];
                    int cn = ClipTileToBox(quad, box, clipped);

                    if (cn >= 3)
                    {
                        tri->Begin(TRI_TRIANGLE_FAN);

                        for (int ci = 0; ci < cn; ci++)
                        {
                            tri->TexCoord2f(clipped[ci].uv.x, clipped[ci].uv.y);
                            tri->Vertex3f(clipped[ci].pos.x, clipped[ci].pos.y, 0.0f);
                        }

                        tri->End();
                    }
                }
            }

            frame++;
            base = base + t.up;
        }

        row = row + t.right;
    }

    if (masked)
    {
        RadarDraw_End();
    }

    tri->RenderMode(kRenderNormal);
}


// The hide/dead gates reflect the local living player's state; while observing
// another player they do not apply, so the radar follows the observed target.
bool HudRadarAdvanced::ShouldDraw(bool spectating)
{
    if (spectating)
    {
        return true;
    }

    if (m_iHideHUDDisplay & HIDEHUD_HEALTH)
    {
        return false;
    }

    // The player is alive and in play once they carry the suit, and that bit is set
    // on every spawn (mid-round CSDM respawns included). m_fPlayerDead is not used
    // here: the stock client freezes it while observing in first person and only
    // clears it on the next CurWeapon, so after a respawn out of an observer it can
    // stay stale-dead until the round restarts, wrongly hiding the radar.
    if (!(m_iWeaponBits & (1 << WEAPON_SUIT)))
    {
        return false;
    }

    // On mp_fadetoblack 1 the just-killed player stays on the corpse cam with
    // g_iUser1 still 0, so the gates here still apply; hide the radar while the
    // player is actually dead so it does not paint live positions over the black
    // fade. Health, not m_fPlayerDead, gates this: m_fPlayerDead can stay stale-true
    // after a respawn out of an observer slot, which would wrongly keep it hidden.
    if (gHUD()->m_Health->m_iHealth <= 0)
    {
        return false;
    }

    // Servers toggle this stock-client flag via the 'hideradar' / 'drawradar'
    // commands; honour it like the classic radar path does.
    if (!gHUD()->m_Health->m_bDrawRadar)
    {
        return false;
    }

    return true;
}

// Per-frame context (who we are / observe), reused by every marker draw so they do
// not each re-query the engine. Drives the border colour and the shared visibility
// set for the objective LOS gating and the enemy gating in DrawPlayers.
void HudRadarAdvanced::UpdateFrameContext(bool spectating, float flTime)
{
    ctx_ = FrameContext{};
    ctx_.spectating = spectating;

    if (cl_entity_t* local = cl_enginefunc()->GetLocalPlayer())
    {
        ctx_.local = local;
        ctx_.self_idx = local->index;
        ctx_.highlight_idx = (ctx_.spectating && g_iUser2 > 0 && g_iUser2 <= MAX_PLAYERS) ? g_iUser2 : ctx_.self_idx;
        ctx_.highlight_team =
            (ctx_.highlight_idx >= 1 && ctx_.highlight_idx <= MAX_PLAYERS) ? cl()->g_PlayerExtraInfo[ctx_.highlight_idx].teamnumber : 0;
        ctx_.local_team = cl()->g_PlayerExtraInfo[ctx_.self_idx].teamnumber;
    }

    // The policy answers who still counts as a teammate this frame, and the visibility
    // pass builds its viewer and target sets from that answer, so it runs in between.
    UpdatePolicy(flTime);

    if (ctx_.local)
    {
        visibility_.Update(ctx_.self_idx, ctx_.highlight_team, ctx_.spectating, flTime, policy_);
    }
}

void HudRadarAdvanced::UpdatePolicy(float flTime)
{
    policy_.SetLogging(RadarDebug_Enabled());
    policy_.SetForceAllyGating(g_RadarCfg.allies->value == 0.0f);

    const RadarMapObjectives& map = objectives_.map_objectives();
    policy_.SetMapObjectives(map.site_count(), map.hostage_count());

    if (flTime - last_hint_read_ >= kHintInterval || flTime < last_hint_read_)
    {
        last_hint_read_ = flTime;
        policy_.SetServerFfaHint(std::atoi(ServerInfoValue("nc_ffa")) != 0);
        policy_.SetBombHint(ParseMessageHint(ServerInfoValue("nc_bombmsg"), "bomb"));
        policy_.SetHostageHint(ParseMessageHint(ServerInfoValue("nc_hostagemsg"), "hostage"));
    }

    float run_speed = (pmove && pmove->movevars) ? pmove->movevars->maxspeed : kRunSpeedFallback;
    bool applicable = ctx_.local != nullptr && !ctx_.spectating && ctx_.local_team != 0;
    policy_.BeginFrame(flTime, run_speed, applicable);

    // The objective messages are classified for spectators too: what they mean does not
    // depend on which team is watching.
    const RadarBroadcast::Bomb& bomb = RadarBroadcast::GetBomb();
    policy_.ObserveBomb({bomb.seq, bomb.active, bomb.pos, bomb.planted, bomb.time}, flTime);

    RadarBroadcast::Hostage hostages[kMaxBroadcastHostages];
    int hostage_count = RadarBroadcast::CollectHostages(hostages, kMaxBroadcastHostages);

    // The engine sends HostagePos to living Counter-Terrorists only, so nobody else can
    // conclude anything from its absence.
    policy_.ObserveHostageBroadcast(!ctx_.spectating && ctx_.local_team == TEAM_CT, hostage_count);

    for (int i = 0; i < hostage_count; i++)
    {
        policy_.ObserveHostage({hostages[i].index, hostages[i].seq, hostages[i].time}, flTime);
    }

    if (!applicable)
    {
        return;
    }

    int message_num = ctx_.local->curstate.messagenum;
    int max_clients = std::min<int>(MAX_PLAYERS, cl_enginefunc()->GetMaxClients());

    for (int i = 1; i <= max_clients; i++)
    {
        if (i == ctx_.self_idx)
        {
            continue;
        }

        extra_player_info_t& ext = cl()->g_PlayerExtraInfo[i];

        if (ext.dead || ext.teamnumber != ctx_.local_team)
        {
            continue;
        }

        cl_entity_t* ent = cl_enginefunc()->GetEntityByIndex(i);
        bool live = ent && ent->player && ent->curstate.messagenum == message_num;
        ncl_math::Vector2 live_pos = live ? ncl_math::Vector2{ent->origin[0], ent->origin[1]} : ncl_math::Vector2{};

        policy_.ObserveAlly(i, live, live_pos, ncl_math::Vector3::Ref(ext.origin));
    }
}

void HudRadarAdvanced::Draw(float flTime)
{
    if (!RadarConfig_AdvancedActive())
    {
        return;
    }

    bool spectating = g_iUser1 != 0 || cl_enginefunc()->IsSpectateOnly();

    if (!ShouldDraw(spectating))
    {
        return;
    }

    bool debug = RadarDebug_Enabled();
    std::chrono::steady_clock::time_point debug_start;

    RadarDraw_ResetStats();
    RadarText_ResetStats();
    visibility_.ResetStats();

    if (debug)
    {
        debug_start = std::chrono::steady_clock::now();
    }

    GetAllPlayersInfo();
    players_.BeginFrame(flTime);

    overview_.EnsureLoaded(RadarMapMode_FromCvar(g_RadarCfg.map_mode->value));

    if (!overview_.valid())
    {
        return;
    }

    RadarBox box = ComputeBox();
    HudTextZone::SetZone(box.x, box.y, box.size); // reserve this area from HUD text
    RadarTransform t = ComputeTransform(box);

    RadarDump::BeginFrame(flTime, box.x, box.y, box.size); // no-op unless radar_dump requested

    UpdateFrameContext(spectating, flTime);

    // A flashbang washes the whole radar by tinting every primitive toward the fade
    // colour as it is drawn, so the interior, the border ring and the markers that
    // protrude past the rim all blind together with the view, without an overlay
    // fill (which would leave a fading halo on the backdrop around the radar).
    ncl_math::Color flash_col;
    flash_alpha_ = FlashAlpha(flash_col);

    if (flash_alpha_ > 0)
    {
        RadarDraw_SetFade(flash_col.WithAlpha(flash_alpha_));
    }
    else
    {
        RadarDraw_ClearFade();
    }

    RadarFrame frame{box, t, flTime, (float)m_flTimeDelta, ctx_, flash_alpha_, g_RadarCfg.round->value != 0.0f};

    DrawBackground(box);

    // Commit the background disc (queued in the prim batch) before the map draws: the
    // no-shader tile path draws through the engine TriAPI, which never triggers the
    // program bind that would otherwise flush the batch, so the disc would land over
    // the map at the next flush instead of under it.
    RadarDraw_FlushPrims();

    if (overview_.runtime())
    {
        DrawRuntimeMap(box, t);
    }
    else
    {
        DrawOverviewTiles(box, t);
    }

    DrawSmoke(box, t, flTime);
    DrawBorder(box);
    // One entity-list sweep per frame feeds both the bomb and the hostage draws.
    objectives_.CollectEntities(frame);
    // The bomb/hostage objective draws each line-of-sight test world points; set up
    // the player-prediction trace context once for the whole batch.
    visibility_.BeginObjectiveTests();
    objectives_.DrawBomb(frame); // under the bomb-site letters
    objectives_.DrawSites(frame);
    objectives_.DrawHostages(frame);
    visibility_.EndObjectiveTests();
    players_.DrawPlayers(frame);
    players_.DrawDeaths(frame);

    RadarDraw_ClearFade();
    RadarDump::EndFrame();

    if (debug)
    {
        RadarDrawStats stats = RadarDraw_Stats();
        float total_ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - debug_start).count();
        RadarDebug_Frame(box, total_ms, stats.blur_ms, visibility_.traces_ms(), RadarText_Ms(), stats.batches);
    }
}

int HudRadarAdvanced::FlashAlpha(ncl_math::Color& color)
{
    // The engine blends the screen fade onto the 3D scene before the HUD is drawn,
    // so a flashbang leaves the radar fully readable. Read the current fade so the
    // radar can be washed out together with the view (see RadarDraw_SetFade).
    screenfade_t fade;
    std::memset(&fade, 0, sizeof(fade));
    cl_enginefunc()->pfnGetScreenFade(&fade);

    // Only a near-white flashbang washes the radar; ignore fade-to-black and other
    // tinted fades (death, round transitions). Shares the predicate with the enemy
    // line-of-sight gating so the two agree on what counts as a flash.
    if (!RadarFade::NearWhite(fade))
    {
        return 0;
    }

    int a = std::min(RadarFade::Alpha(fade, cl_enginefunc()->GetClientTime()), 255);

    if (a <= 0)
    {
        return 0;
    }

    color.r = fade.fader;
    color.g = fade.fadeg;
    color.b = fade.fadeb;

    return a;
}

void HudRadarAdvanced::DrawSmoke(const RadarBox& box, const RadarTransform& t, float time)
{
    RadarSmoke::Disc smokes[16];
    int n = RadarSmoke::Collect(smokes, 16, time);

    if (n == 0)
    {
        return;
    }

    bool round = g_RadarCfg.round->value != 0.0f;
    float cx = box.x + box.size * 0.5f;
    float cy = box.y + box.size * 0.5f;
    RadarDraw_SetPrimClip({cx, cy}, box.size * 0.5f, !round);
    RadarDraw_BeginPrims();

    for (int i = 0; i < n; i++)
    {
        ncl_math::Vector2 s = t.WorldToScreen({smokes[i].center.x, smokes[i].center.y});
        ncl_math::Vector2 e = t.WorldToScreen({smokes[i].center.x + smokes[i].radius, smokes[i].center.y});
        float sr = std::max(2.0f, (e - s).Length());
        RadarDump::AddMarker("smoke", i, smokes[i].center.x, smokes[i].center.y, s.x, s.y, 210, 210, 210, 110, "");
        RadarDraw_DrawDisc(s, sr, {210, 210, 210, 110});
    }

    RadarDraw_EndPrims();
    RadarDraw_ClearPrimClip();
}

void HudRadarAdvanced::DrawBorder(const RadarBox& box)
{
    // The rim takes the current (or observed) player's team colour: a saturated gold
    // for terrorists, light blue for counter-terrorists, and the neutral warm tone
    // when there is no team (free-look spectator, warmup).
    ncl_math::Color rim{230, 220, 150};

    if (ctx_.highlight_team == TEAM_TERRORIST)
    {
        rim = {235, 215, 95};
    }
    else if (ctx_.highlight_team == TEAM_CT)
    {
        rim = {120, 190, 255};
    }

    const byte ca = 255;
    const int thickness = 1;

    float cx = box.x + box.size * 0.5f;
    float cy = box.y + box.size * 0.5f;

    if (g_RadarCfg.round->value != 0.0f)
    {
        float outer = box.size * 0.5f;
        float inner = outer - (float)thickness - 0.5f;
        RadarDraw_DrawBorderRing({cx, cy}, outer, inner, 1.0f, rim.WithAlpha(ca));
        return;
    }

    int x1 = (int)box.x;
    int y1 = (int)box.y;
    int w = (int)box.size;

    ncl_math::Color f = RadarDraw_FadeColor(rim);
    FillRGBABlend(x1, y1, w, thickness, f.r, f.g, f.b, ca);
    FillRGBABlend(x1, y1 + w - thickness, w, thickness, f.r, f.g, f.b, ca);
    FillRGBABlend(x1, y1, thickness, w, f.r, f.g, f.b, ca);
    FillRGBABlend(x1 + w - thickness, y1, thickness, w, f.r, f.g, f.b, ca);
}
