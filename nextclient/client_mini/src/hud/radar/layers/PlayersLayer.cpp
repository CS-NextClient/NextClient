#include "PlayersLayer.h"

#include <algorithm>
#include <ranges>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "main.h"

#include <strtools.h>
#include <ncl_math/vec3.h>
#include <next_client_mini/radar_cvars.h>
#include <goldsrc/client_api.h>
#include <goldsrc/cstrike/hud.h>
#include <goldsrc/cvar.h>
#include <goldsrc/entity.h>
#include <goldsrc/pmove.h>

#include "hud/radar/RadarConfig.h"
#include "hud/radar/RadarDraw.h"
#include "hud/radar/RadarLabel.h"
#include "hud/radar/RadarPalette.h"
#include "hud/radar/RadarPolicy.h"
#include "hud/radar/RadarText.h"
#include "hud/radar/Visibility.h"
#include "view.h"

namespace
{
    // Fractions of box.marker_px.
    constexpr float kPlayerDotScale = 0.023f;
    constexpr float kDeathCrossScale = 0.02f;
    constexpr float kTrackWidth = 2.0f;

    constexpr float kRunSpeedFallback = 320.0f; // stock sv_maxspeed
    constexpr float kTeleportPad = 24.0f;
    constexpr float kTrailAlphaTail = 30.0f;
    constexpr float kTrailAlphaHead = 200.0f;

    constexpr float kNameScale = 0.85f;
    constexpr float kNameHeightScale = 0.055f; // fraction of box.marker_px
    constexpr int kNameMinPx = 9;
    constexpr int kNameMaxPx = 28;

    // Codepoints, not bytes.
    constexpr int kMaxNameChars = 16;

    constexpr RadarCvarDef kHealthSizeCvar = RadarCvarDef_Get("hud_radar_health_size");
    static_assert(kHealthSizeCvar.name != nullptr);

    constexpr RadarCvarDef kNameSizeCvar = RadarCvarDef_Get("hud_radar_name_size");
    static_assert(kNameSizeCvar.name != nullptr);

    bool IsKnownOrigin(const ncl_math::Vector3& o)
    {
        return o.x != 0.0f || o.y != 0.0f || o.z != 0.0f;
    }
} // namespace

PlayersLayer::PlayersLayer(RadarVisibility& visibility, const RadarPolicy& policy, RadarText& text) :
    visibility_(visibility),
    policy_(policy),
    text_(text)
{
}

void PlayersLayer::BeginFrame(float time)
{
    tracks_.BeginFrame(time);
}

void PlayersLayer::Draw(const RadarFrame& frame)
{
    DrawPlayers(frame);
    DrawDeaths(frame);
}

void PlayersLayer::OnPlayerRespawn()
{
    tracks_.Reset();
    prev_alive_.fill(false);
    deaths_.Reset();
}

void PlayersLayer::OnVidInit()
{
    name_widths_.fill(NameWidth{}); // the console font scales with the video mode
}

void PlayersLayer::DrawMarker(ncl_math::Vector2 pos, ncl_math::Vector2 dir, int size, ncl_math::Color color, float halo_radius)
{
    // A zero dir draws the dot alone.
    ncl_math::Vector2 fwd{};
    bool is_dir = ncl_math::TryNormalize(dir, &fwd);

    if (is_dir)
    {
        // Both base corners sit on the dot's rim (or the carrier halo's), so the head never
        // juts out the side or back of the marker.
        float cut = std::max((float)size, halo_radius);
        float spread = 0.62f; // base half-angle, radians
        float tip_d = cut + size * 1.0f;
        ncl_math::Vector2 tip = pos + fwd * tip_d;
        ncl_math::Vector2 base_l = pos + fwd.Rotated(spread) * cut;
        ncl_math::Vector2 base_r = pos + fwd.Rotated(-spread) * cut;
        RadarDraw_DrawTriangle(tip, base_l, base_r, {240, 240, 240, color.a});
    }

    RadarDraw_DrawDisc(pos, (float)size, color);
}

void PlayersLayer::DrawEdgeArrow(ncl_math::Vector2 pos, ncl_math::Vector2 out_dir, int size, ncl_math::Color color)
{
    ncl_math::Vector2 fwd;

    // Cannot happen off the rim, but a plain dot beats a zero-area triangle.
    if (!ncl_math::TryNormalize(out_dir, &fwd))
    {
        RadarDraw_DrawDisc(pos, (float)size, color);
        return;
    }

    ncl_math::Vector2 perp = fwd.Perp();

    ncl_math::Vector2 tip = pos + fwd * (size * 1.7f);
    ncl_math::Vector2 base = pos - fwd * (size * 0.8f);
    ncl_math::Vector2 base_l = base + perp * (size * 1.2f);
    ncl_math::Vector2 base_r = base - perp * (size * 1.2f);

    RadarDraw_DrawTriangle(tip, base_l, base_r, color);
}

void PlayersLayer::DrawTrack(const RadarFrame& frame, int player, ncl_math::Vector2 pos, ncl_math::Color color)
{
    tracks_.Sample(player, pos, frame.time);

    ncl_math::Vector2 world[PlayerTracks::kMaxPoints];
    int count = tracks_.CopyPoints(player, world);

    if (count < 2)
    {
        return;
    }

    ncl_math::Vector2 pts[PlayerTracks::kMaxPoints];
    uint8_t al[PlayerTracks::kMaxPoints];

    for (int k = 0; k < count; k++)
    {
        pts[k] = frame.shape.Clamp(1, frame.t.WorldToScreen(world[k]));
        al[k] = (uint8_t)ncl_math::Lerp(kTrailAlphaTail, kTrailAlphaHead, (float)k / (float)(count - 1));
    }

    RadarDraw_DrawPolyline({pts, (size_t)count}, {al, (size_t)count}, color, kTrackWidth);
}

void PlayersLayer::RecordPlayerDeathIfNew(int player, RadarTeam team, float time)
{
    if (!prev_alive_[player])
    {
        return;
    }

    prev_alive_[player] = false;

    if (team == RadarTeam::Terrorist || team == RadarTeam::CounterTerrorist)
    {
        const ncl_math::Color& dc = (team == RadarTeam::Terrorist) ? RadarPalette::kTerrorist : RadarPalette::kCounterTerrorist;
        deaths_.Add(prev_pos_[player], dc, time);
    }
}

int PlayersLayer::ComputePlayerNameWidth(int player, const char* name)
{
    NameWidth& e = name_widths_[player];

    if (std::strcmp(e.name, name) != 0)
    {
        V_strcpy_safe(e.name, name);
        int width = 0, height = 0;
        gEngfuncs.pfnDrawConsoleStringLen(name, &width, &height);
        e.width = width;
    }

    return e.width;
}

void PlayersLayer::DrawHealthBar(ncl_math::Vector2 pos, int dot, int sb_health)
{
    float sx = pos.x, sy = pos.y;
    int hp = std::clamp(sb_health, 0, 100);
    float hsz = std::clamp(g_RadarCfg.health_size->value, kHealthSizeCvar.min_value, kHealthSizeCvar.max_value);
    int bw = std::max(2, (int)(dot * 3 * hsz));
    int bh = std::max(1, (int)(2 * hsz));
    int bx = (int)sx - bw / 2;
    int by = (int)sy - dot - bh - 1;
    ncl_math::Color hp_col;

    if (hp > 60)
    {
        hp_col = {0, 200, 0};
    }
    else if (hp > 30)
    {
        hp_col = {230, 200, 0};
    }
    else
    {
        hp_col = {230, 40, 40};
    }

    // The shader applies the flashbang wash, so the colours are passed through unfaded.
    RadarDraw_DrawRect({(float)(bx - 1), (float)(by - 1)}, {(float)(bx + bw + 1), (float)(by + bh + 1)}, {0, 0, 0, 150});
    RadarDraw_DrawRect({(float)bx, (float)by}, {(float)(bx + bw), (float)(by + bh)}, {30, 30, 30, 200});
    RadarDraw_DrawRect({(float)bx, (float)by}, {(float)(bx + bw * hp / 100), (float)(by + bh)}, hp_col);
}

void PlayersLayer::DrawPlayers(const RadarFrame& frame)
{
    if (!frame.ctx.local)
    {
        return;
    }

    int dot = std::max(2, (int)(frame.box.marker_px * kPlayerDotScale));
    std::array<PlayerJob, kRadarMaxPlayers + 1> jobs;
    DrawPlayerLayers(frame, CollectPlayerJobs(frame, dot, jobs), dot, ComputeHighlightConeSpread(frame));
}

float PlayersLayer::ComputeHighlightConeSpread(const RadarFrame& frame)
{
    float fov = 90.0f;

    if (gHUD && gHUD->m_iFOV && *gHUD->m_iFOV > 1)
    {
        fov = (float)*gHUD->m_iFOV;
    }

    if (frame.ctx.spectating && gHUD && gHUD->m_pPlayerFOV && frame.ctx.highlight_idx >= 1 && frame.ctx.highlight_idx < 64)
    {
        int pf = (*gHUD->m_pPlayerFOV)[frame.ctx.highlight_idx];

        if (pf > 1)
        {
            fov = (float)pf;
        }
    }

    return std::clamp(fov, 10.0f, 130.0f) * 0.5f * ncl_math::kDeg2Rad;
}

bool PlayersLayer::ResolvePlayerSpot(const RadarFrame& frame, int i, bool is_self, goldsrc::cl_entity_t* ent, PlayerSpot& out)
{
    goldsrc::cl_entity_t* local = frame.ctx.local;
    goldsrc::cstrike::extra_player_info_t& ext = client()->g_PlayerExtraInfo[i];
    RadarTeam team = RadarTeam_From(ext.teamnumber);
    bool live = ent && ent->player && ent->curstate.messagenum == local->curstate.messagenum;
    bool is_ally = is_self || (team == frame.ctx.highlight_team && frame.ctx.highlight_team != RadarTeam::Unassigned);

    if (is_ally && !is_self && !policy_.IsAllyRevealed(i))
    {
        is_ally = false;
    }

    const float* origin = nullptr;
    float spot_xy[3] = {};
    float facing_yaw = 0.0f;
    bool has_dir = false;
    int marker_alpha = 255;

    if (is_self)
    {
        origin = local->origin;
        facing_yaw = g_MainViewAngles[1];
        has_dir = true;
    }
    else if (is_ally)
    {
        if (live)
        {
            origin = ent->origin;
            facing_yaw = ent->angles[1];
            has_dir = true;
        }
        else
        {
            origin = ext.origin; // teammate radar broadcast
        }
    }
    else if (visibility_.is_gating())
    {
        // Use the spotted position/facing, never the live one: the marker must freeze
        // the instant sight is lost.
        ncl_math::Vector2 p;
        float a = 1.0f, yaw = 0.0f;

        if (!visibility_.ShouldShowEnemy(i, frame.time, p, yaw, a))
        {
            return false;
        }
        marker_alpha = (int)(a * 255.0f);
        spot_xy[0] = p.x;
        spot_xy[1] = p.y;
        origin = spot_xy;
        facing_yaw = yaw;
        has_dir = true;
    }
    else if (live)
    {
        origin = ent->origin;
        facing_yaw = ent->angles[1];
        has_dir = true;
    }
    else
    {
        return false;
    }

    if (!IsKnownOrigin(ncl_math::Vector3::Ref(origin)))
    {
        return false;
    }

    out.pos = {origin[0], origin[1]};
    out.yaw = facing_yaw;
    out.alpha = marker_alpha;
    out.has_dir = has_dir;
    return true;
}

bool PlayersLayer::IsTeleported(int player, ncl_math::Vector2 pos, float run_speed, float dt) const
{
    if (!prev_alive_[player])
    {
        return false;
    }

    float frame_move = run_speed * std::clamp(dt, 0.005f, 0.1f) + kTeleportPad;

    return (pos - prev_pos_[player]).LengthSqr() > frame_move * frame_move;
}

void PlayersLayer::UpdatePlayerTrail(const RadarFrame& frame,
                                     int player,
                                     const goldsrc::cl_entity_t* ent,
    ncl_math::Vector2 world, ncl_math::Color color, bool offscreen, bool teleported)
{
    // An offscreen player's samples would all clamp onto the rim as a squiggle under the
    // edge arrow; the trail is left intact across the gap so it resumes on return.
    if (player == frame.ctx.highlight_idx)
    {
        tracks_.Clear(player);
        return;
    }

    if (g_RadarCfg.tracks->value == 0.0f || offscreen)
    {
        return;
    }

    if (teleported || (ent && (ent->curstate.effects & goldsrc::EF_NOINTERP)))
    {
        tracks_.Clear(player);
    }

    if (tracks_.IsSuppressed(frame.time))
    {
        tracks_.Clear(player);
        return;
    }

    DrawTrack(frame, player, world, color);
}

void PlayersLayer::FillPlayerJob(
    const RadarFrame& frame,
    int player,
    bool is_self,
    RadarTeam team,
    const PlayerSpot& spot,
    const goldsrc::cstrike::extra_player_info_t& ext,
    int dot,
    PlayerJob& job
) const
{
    ncl_math::Color col = team == RadarTeam::Terrorist ? RadarPalette::kTerrorist : RadarPalette::kCounterTerrorist;

    if (player == frame.ctx.highlight_idx)
    {
        col = RadarPalette::kHighlight;
    }

    ncl_math::Vector2 marker_raw = frame.t.WorldToScreen(spot.pos);

    bool offscreen = !frame.shape.Contains(marker_raw);
    ncl_math::Vector2 out_dir{};

    if (offscreen)
    {
        ncl_math::TryNormalize(marker_raw - frame.shape.center, &out_dir);
    }

    bool carrier = team == RadarTeam::Terrorist && ext.has_c4 && !offscreen;
    RadarTeam my_team = frame.ctx.highlight_team;

    job.pos = frame.shape.Clamp(dot, marker_raw);
    job.dir = spot.has_dir ? frame.t.MarkerDirection(spot.yaw) : ncl_math::Vector2{};
    job.out_dir = out_dir;
    job.col = col;
    job.alpha = spot.alpha;
    job.cone = (player == frame.ctx.highlight_idx);
    job.carrier = carrier;
    job.halo = carrier ? dot * 1.4f : 0.0f;
    job.offscreen = offscreen;
    // sb_health is broadcast to teammates by the ReGameDLL HealthInfo message
    // (mp_scoreboard_showhealth), unlike .health which only refreshes while aiming at one.
    job.health = !offscreen && g_RadarCfg.health->value != 0.0f && player != frame.ctx.highlight_idx && my_team != RadarTeam::Unassigned &&
                 team == my_team && policy_.IsAllyRevealed(player) && ext.sb_health > 0;
    job.sb_health = ext.sb_health;
    job.name = !offscreen && g_RadarCfg.names->value != 0.0f;
    job.index = player;
    job.is_self = is_self;
}

std::span<const PlayersLayer::PlayerJob> PlayersLayer::CollectPlayerJobs(const RadarFrame& frame, int dot, std::span<PlayerJob> jobs)
{
    int job_count = 0;

    // The client arrays are sized for 64, but the engine caps maxclients at 32 and the
    // slots past it are always empty.
    int max_clients = std::min<int>(kRadarMaxPlayers, gEngfuncs.GetMaxClients());

    // The live limit, so the teleport heuristic holds on a modified sv_maxspeed.
    float run_speed = (pmove && pmove->movevars) ? pmove->movevars->maxspeed : kRunSpeedFallback;

    for (int i = 1; i <= max_clients; i++)
    {
        goldsrc::cstrike::extra_player_info_t& ext = client()->g_PlayerExtraInfo[i];

        if (ext.dead)
        {
            RecordPlayerDeathIfNew(i, RadarTeam_From(ext.teamnumber), frame.time);
            tracks_.Clear(i);
            continue;
        }

        RadarTeam team = RadarTeam_From(ext.teamnumber);
        bool is_self = (i == frame.ctx.self_idx && !frame.ctx.spectating);

        if (team != RadarTeam::CounterTerrorist && team != RadarTeam::Terrorist && !is_self)
        {
            continue;
        }

        goldsrc::cl_entity_t* ent = gEngfuncs.GetEntityByIndex(i);
        PlayerSpot spot;

        if (!ResolvePlayerSpot(frame, i, is_self, ent, spot))
        {
            // Do not clear the trail: a transient PVS gap must resume on return, and the
            // trail's own break distance handles a far re-acquire.
            continue;
        }

        bool teleported = IsTeleported(i, spot.pos, run_speed, frame.dt);

        prev_alive_[i] = true;
        prev_pos_[i] = spot.pos;

        PlayerJob& job = jobs[job_count++];
        FillPlayerJob(frame, i, is_self, team, spot, ext, dot, job);

        UpdatePlayerTrail(frame, i, ent, spot.pos, job.col, job.offscreen, teleported);
    }

    return jobs.first(job_count);
}

void PlayersLayer::DrawPlayerLayers(const RadarFrame& frame, std::span<const PlayerJob> jobs, int dot, float cone_spread)
{
    RadarDraw_BeginPrims();
    for (const PlayerJob& job : jobs)
    {
        if (!job.cone || job.offscreen)
        {
            continue;
        }

        ncl_math::Vector2 fwd;

        if (!ncl_math::TryNormalize(job.dir, &fwd))
        {
            continue;
        }

        ncl_math::Color light = ncl_math::Lerp(job.col.WithAlpha(job.alpha), ncl_math::Color{255, 255, 255}, 2.0f / 3.0f);
        RadarDraw_DrawCone(job.pos, fwd, dot * 6.5f, cone_spread, light, 0.65f * (job.alpha / 255.0f));
    }
    RadarDraw_EndPrims();

    RadarDraw_BeginPrims();
    for (const PlayerJob& job : jobs)
    {
        if (job.offscreen)
        {
            DrawEdgeArrow(job.pos, job.out_dir, dot, job.col.WithAlpha(job.alpha));
            continue;
        }

        if (job.carrier)
        {
            RadarDraw_DrawRing(job.pos, job.halo, (float)dot, RadarPalette::kGold.WithAlpha(job.alpha));
        }

        DrawMarker(job.pos, job.dir, dot, job.col.WithAlpha(job.alpha), job.halo);
    }

    for (const PlayerJob& job : jobs)
    {
        if (job.health)
        {
            DrawHealthBar(job.pos, dot, job.sb_health);
        }
    }
    RadarDraw_EndPrims();

    DrawNames(frame, jobs, dot);
}

int PlayersLayer::ComputeNameHeightPx(const RadarFrame& frame)
{
    float mul = std::clamp(g_RadarCfg.name_size->value, kNameSizeCvar.min_value, kNameSizeCvar.max_value);
    int standard = text_.ResolveStandardPx();

    // kNameMinPx/kNameMaxPx bound the fallback ladder alone: applied to the scheme-derived
    // size they cap below the cvar's range and the top of the settings slider does nothing.
    if (standard > 0)
    {
        return (int)std::lround(standard * kNameScale * mul);
    }

    return std::clamp((int)std::lround(frame.box.marker_px * kNameHeightScale * mul), kNameMinPx, kNameMaxPx);
}

void PlayersLayer::DrawNames(const RadarFrame& frame, std::span<const PlayerJob> jobs, int dot)
{
    bool any_name = std::ranges::any_of(jobs, [](const PlayerJob& job) { return job.name; });

    // Return before touching the font: the GDI face and the atlas must not be built for a
    // player who keeps names off.
    if (!any_name)
    {
        return;
    }

    int name_px = ComputeNameHeightPx(frame);
    bool batched = text_.CheckAvailable(name_px);

    float flash_fade = RadarLabel_ComputeFlashFadeFor(batched, frame.flash_alpha);

    if (flash_fade <= 0.0f)
    {
        return;
    }

    if (batched)
    {
        RadarDraw_BeginPrims();
    }

    for (const PlayerJob& job : jobs)
    {
        if (!job.name)
        {
            continue;
        }

        if (job.is_self)
        {
            continue;
        }
        
        goldsrc::hud_player_info_t info;
        std::memset(&info, 0, sizeof(info));
        gEngfuncs.pfnGetPlayerInfo(job.index, &info);

        if (!info.name || !info.name[0])
        {
            continue;
        }

        char name[80];
        V_strcpy_safe(name, info.name);
        Q_UnicodeTruncate(name, kMaxNameChars);

        if (batched)
        {
            int alpha = (int)(job.alpha * flash_fade);
            text_.DrawCentered(job.index, name, {job.pos.x, job.pos.y + dot + 1}, name_px, job.col.WithAlpha(alpha));
        }
        else
        {
            int tw = ComputePlayerNameWidth(job.index, name);
            RadarLabel_Draw(name, (int)job.pos.x - tw / 2, (int)job.pos.y + dot + 1, job.col, frame.flash_alpha);
        }
    }

    if (batched)
    {
        RadarDraw_EndPrims();
    }
}

void PlayersLayer::DrawDeaths(const RadarFrame& frame)
{
    DeathMarks::Visible marks[DeathMarks::kMax];
    int n = deaths_.CollectVisible(marks, frame.time);
    float sz = std::max(2.0f, frame.box.marker_px * kDeathCrossScale);

    RadarDraw_BeginPrims();

    for (int i = 0; i < n; i++)
    {
        ncl_math::Vector2 s = frame.shape.Clamp((int)sz, frame.t.WorldToScreen(marks[i].pos));

        // Half-width scales with the cross so it does not look thin at high resolutions.
        float hw = std::max(1.3f, sz * 0.42f);
        RadarDraw_DrawCross(s, sz, hw, marks[i].color.WithAlpha(marks[i].alpha));
    }

    RadarDraw_EndPrims();
}
