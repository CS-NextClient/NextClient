#include "PlayersLayer.h"

#include <algorithm>
#include <ranges>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "main.h"

#include "hud/radar/integration/Performance.h"

#include <strtools.h>
#include <ncl_math/vec3.h>
#include <next_client_mini/radar_cvars.h>
#include <goldsrc/client_api.h>
#include <goldsrc/cstrike/hud.h>
#include <goldsrc/cvar.h>
#include <goldsrc/entity.h>
#include <goldsrc/pmove.h>

#include "hud/radar/integration/RadarConfig.h"
#include "hud/radar/integration/RadarDebug.h"
#include "hud/radar/layers/players/PlayerObserver.h"
#include "hud/radar/markers/RadarBroadcast.h"
#include "hud/radar/render/RadarDraw.h"
#include "hud/radar/render/text/RadarLabel.h"
#include "hud/radar/markers/RadarLayerMarker.h"
#include "hud/radar/render/RadarPalette.h"
#include "hud/radar/policy/RadarPolicy.h"
#include "hud/radar/render/text/RadarText.h"
#include "hud/radar/policy/Visibility.h"
#include "view.h"

namespace
{
    // Fractions of box.marker_px.
    constexpr float kPlayerDotScale = 0.026f;
    constexpr float kCarrierRingWidthScale = 0.15f;
    constexpr float kMemoryMarkScale = 0.025f;
    constexpr float kMemoryMarkMinSize = 2.5f;
    constexpr float kMemoryMarkWidthScale = 0.36f;
    constexpr float kMemoryMarkMinHalfWidth = 1.15f;
    constexpr float kQuestionMarkWidthFactor = 0.9f;
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

    constexpr cvars::CvarDefinition kHealthSizeCvar = radar::FindCvar("hud_radar_health_size");
    static_assert(kHealthSizeCvar.name != nullptr);

    constexpr cvars::CvarDefinition kNameSizeCvar = radar::FindCvar("hud_radar_name_size");
    static_assert(kNameSizeCvar.name != nullptr);

    const char* ResetReasonName(PlayersLayer::ResetReason reason)
    {
        switch (reason)
        {
            case PlayersLayer::ResetReason::Hud:
                return "hud";
            case PlayersLayer::ResetReason::InitHud:
                return "init_hud";
            case PlayersLayer::ResetReason::Video:
                return "video";
            case PlayersLayer::ResetReason::PlayerClock:
                return "player_clock";
            case PlayersLayer::ResetReason::RadarClock:
                return "radar_clock";
            case PlayersLayer::ResetReason::Team:
                return "team";
            case PlayersLayer::ResetReason::Observer:
                return "observer";
            case PlayersLayer::ResetReason::Count:
                break;
        }
        return "unknown";
    }

    bool IsKnownOrigin(const ncl_math::Vector3& o)
    {
        return o.x != 0.0f || o.y != 0.0f || o.z != 0.0f;
    }
} // namespace

PlayersLayer::PlayersLayer(
    RadarDrawContext& draw,
    radar::integration::Performance& performance,
    RadarVisibility& visibility,
    const RadarPolicy& policy,
    const RadarBroadcast& broadcast,
    RadarText& text
) :
    draw_(draw),
    performance_(performance),
    visibility_(visibility),
    policy_(policy),
    text_(text),
    broadcast_(broadcast)
{
}

void PlayersLayer::BeginFrame(float time)
{
    if (time < last_time_)
    {
        OnPlayerRespawn(ResetReason::PlayerClock, last_time_, time);
    }
    
    last_time_ = time;
    drawn_.fill(false);
    fading_players_ = 0;
    broadcast_players_ = 0;
    fading_in_players_ = 0;
    overlapping_players_ = 0;
    snapshot_step_players_ = 0;
    tracks_.BeginFrame(time);
}

void PlayersLayer::Draw(const RadarFrame& frame)
{
    performance::PerformanceCpuScope scope(performance_.get_monitor(), performance::PerfStage::RadarPlayers);
    if (team_ != frame.ctx.highlight_team || spectating_ != frame.ctx.spectating)
    {
        OnPlayerRespawn(spectating_ != frame.ctx.spectating ? ResetReason::Observer : ResetReason::Team, last_time_, frame.time);
        tracks_.BeginFrame(frame.time);
        team_ = frame.ctx.highlight_team;
        spectating_ = frame.ctx.spectating;
    }

    if (frame.ctx.spectating)
    {
        DrawObserver(frame);
        DrawObserverStatus(frame);
        return;
    }

    DrawPlayers(frame);
    DrawLastSeen(frame);
    DrawDeaths(frame);
}

void PlayersLayer::DrawObserver(const RadarFrame& frame)
{
    int player = frame.ctx.observed_idx;
    const goldsrc::cl_entity_t* entity = nullptr;
    bool alive = false;
    
    if (!frame.ctx.free_camera && player >= 1 && player <= std::min<int>(kRadarMaxPlayers, gEngfuncs.GetMaxClients()))
    {
        entity = gEngfuncs.GetEntityByIndex(player);
        alive = !client()->g_PlayerExtraInfo[player].dead;
    }
    
    PlayerObserverMarker marker =
        PlayerObserver_ChooseMarker(frame.ctx, entity, alive, ncl_math::Vector3::Ref(g_MainViewOrigin), g_MainViewAngles[1]);
    
    if (marker.player != 0)
    {
        prev_pos_[marker.player] = marker.pos;
        drawn_[marker.player] = true;
    }

    int dot = std::max(2, (int)(frame.box.marker_px * kPlayerDotScale));
    RadarLayerStyle style = RadarLayerMarker_ComputeStyle(frame.layer_view, marker.pos);
    int extent = frame.layer_view.enabled ? dot * 2 : dot;
    ncl_math::Vector2 raw = frame.t.WorldToScreen({marker.pos.x, marker.pos.y});
    ncl_math::Vector2 dir = frame.t.MarkerDirection(marker.yaw);
    RadarPlayerPlacement player_placement = RadarLayerMarker_PlacePlayer(
        frame.shape,
        raw,
        dir,
        dot,
        extent,
        style
    );
    
    const RadarLayerPlacement& placement = player_placement.placement;
    ncl_math::Vector2 pos = placement.marker;
    ncl_math::Color color = RadarPalette::kHighlight.WithAlpha((int)(255 * style.opacity));

    draw_.BeginPrims();

    if (placement.offscreen)
    {
        DrawEdgeArrow(pos, player_placement.out_dir, dot, color);
    }
    else
    {
        draw_.DrawCone(pos, dir, dot * 6.5f, ComputeHighlightConeSpread(frame), color, 0.65f * style.opacity);
        DrawMarker(pos, dir, dot, color);
    }

    draw_.DrawLayerIndicator(placement.indicator, style.vertical, dot, color);
    draw_.EndPrims();
}

void PlayersLayer::DrawObserverStatus(const RadarFrame& frame)
{
    PlayerObserverIcon icon = PlayerObserver_BuildStatusIcon(frame.box, frame.shape);
    draw_.BeginPrims();
    draw_.DrawDisc(icon.center, icon.radius, {0, 0, 0, 190});
    draw_.DrawDisconnectedSignal(icon.center, icon.half_extent, icon.line_width * 0.5f, {255, 205, 100, 255});
    draw_.EndPrims();
}

void PlayersLayer::OnPlayerRespawn(ResetReason reason, float previous_time, float time)
{
    // Keep diagnostic history across resets so a mass fade can be traced to its reset.
    marker_resets_++;
    reset_counts_[(size_t)reason]++;
    reset_reason_ = reason;
    reset_previous_time_ = previous_time;
    reset_time_ = time;
    
    if (RadarDebug_IsEnabled())
    {
        gEngfuncs.Con_Printf(
            "radar reset: count=%d reason=%s previous_time=%.6f time=%.6f message=%d\n",
            marker_resets_,
            ResetReasonName(reason),
            previous_time,
            time,
            eng()->client_state->parsecount
        );
    }
    
    tracks_.Reset();
    last_seen_.Reset();
    marker_fade_.fill(PlayerMarkerFade{});
    prev_alive_.fill(false);
    prev_snapshot_.fill(false);
    drawn_.fill(false);
    deaths_.Reset();
}

void PlayersLayer::DumpMarkers() const
{
    gEngfuncs.Con_Printf(
        "radar resets: total=%d hud=%d init_hud=%d video=%d player_clock=%d radar_clock=%d team=%d observer=%d "
        "last=%s previous_time=%.6f time=%.6f\n",
        marker_resets_,
        reset_counts_[(size_t)ResetReason::Hud],
        reset_counts_[(size_t)ResetReason::InitHud],
        reset_counts_[(size_t)ResetReason::Video],
        reset_counts_[(size_t)ResetReason::PlayerClock],
        reset_counts_[(size_t)ResetReason::RadarClock],
        reset_counts_[(size_t)ResetReason::Team],
        reset_counts_[(size_t)ResetReason::Observer],
        ResetReasonName(reset_reason_),
        reset_previous_time_,
        reset_time_
    );
    std::array<PlayerMarkerHistory::Sample, PlayerMarkerHistory::kMaxSamples> samples{};
    
    int sample_count = marker_history_.CopySamples(samples);
    float span = sample_count > 0 ? samples[sample_count - 1].frame.time - samples[0].frame.time : 0.0f;
    gEngfuncs.Con_Printf("radar marker history: samples=%d span=%.3f; enable hud_radar_debug 1 before reproducing\n", sample_count, span);
    
    for (int sample = 0; sample < sample_count; sample++)
    {
        PlayerMarkerHistory_PrintSample(samples[sample], gEngfuncs.Con_Printf);
    }
    
    int message_num = eng()->client_state->parsecount;
    float time = gEngfuncs.GetClientTime();
    gEngfuncs.Con_Printf("radar markers: time=%.3f drawn_time=%.3f message=%d spectating=%d\n", time, last_time_, message_num, spectating_);
    
    int max_clients = std::min<int>(kRadarMaxPlayers, gEngfuncs.GetMaxClients());
    
    for (int i = 1; i <= max_clients; i++)
    {
        const goldsrc::cl_entity_t* entity = gEngfuncs.GetEntityByIndex(i);
        
        if (!entity || !entity->player)
        {
            continue;
        }
        
        const RadarBroadcast::Player& report = broadcast_.get_player(i);
        const ncl_math::Vector3& shown = prev_pos_[i];
        gEngfuncs.Con_Printf(
            "player=%d team=%d drawn=%d current=%d messages=%d/%d effects=%d "
            "raw=(%.1f %.1f %.1f) interpolated=(%.1f %.1f %.1f) shown=(%.1f %.1f %.1f) "
            "radar_seq=%u radar_age=%.3f radar=(%.1f %.1f %.1f)\n",
            i,
            client()->g_PlayerExtraInfo[i].teamnumber,
            drawn_[i],
            entity->curstate.messagenum == message_num,
            entity->prevstate.messagenum,
            entity->curstate.messagenum,
            entity->curstate.effects,
            entity->curstate.origin[0],
            entity->curstate.origin[1],
            entity->curstate.origin[2],
            entity->origin[0],
            entity->origin[1],
            entity->origin[2],
            shown.x,
            shown.y,
            shown.z,
            report.seq,
            report.seq != 0 ? time - report.time : -1.0f,
            report.pos.x,
            report.pos.y,
            report.pos.z
        );
    }
}

void PlayersLayer::OnVidInit()
{
    marker_history_.Reset();
    marker_resets_ = 0;
    reset_counts_ = {};
    reset_reason_ = ResetReason::Hud;
    reset_previous_time_ = reset_time_ = 0.0f;
    name_widths_.fill(NameWidth{}); // the console font scales with the video mode
}

void PlayersLayer::DrawMarker(ncl_math::Vector2 pos, ncl_math::Vector2 dir, int size, ncl_math::Color color)
{
    // A zero dir draws the dot alone.
    ncl_math::Vector2 fwd{};
    bool is_dir = ncl_math::TryNormalize(dir, &fwd);

    if (is_dir)
    {
        // Both base corners sit on the dot's rim, so the head never juts out its side or back.
        float cut = (float)size;
        float spread = 0.62f; // base half-angle, radians
        float tip_d = cut + size * 1.0f;
        ncl_math::Vector2 tip = pos + fwd * tip_d;
        ncl_math::Vector2 base_l = pos + fwd.Rotated(spread) * cut;
        ncl_math::Vector2 base_r = pos + fwd.Rotated(-spread) * cut;
        draw_.DrawTriangle(tip, base_l, base_r, {240, 240, 240, color.a});
    }

    draw_.DrawDisc(pos, (float)size, color);
}

void PlayersLayer::DrawEdgeArrow(ncl_math::Vector2 pos, ncl_math::Vector2 out_dir, int size, ncl_math::Color color)
{
    ncl_math::Vector2 fwd;

    // Cannot happen off the rim, but a plain dot beats a zero-area triangle.
    if (!ncl_math::TryNormalize(out_dir, &fwd))
    {
        draw_.DrawDisc(pos, (float)size, color);
        return;
    }

    ncl_math::Vector2 perp = fwd.Perp();

    ncl_math::Vector2 tip = pos + fwd * (size * RadarPlayerMarkerGeometry::kTipScale);
    ncl_math::Vector2 base = pos - fwd * (size * RadarPlayerMarkerGeometry::kBaseScale);
    ncl_math::Vector2 base_l = base + perp * (size * RadarPlayerMarkerGeometry::kHalfWidthScale);
    ncl_math::Vector2 base_r = base - perp * (size * RadarPlayerMarkerGeometry::kHalfWidthScale);

    draw_.DrawTriangle(tip, base_l, base_r, color);
}

void PlayersLayer::DrawTrack(const RadarFrame& frame, int player, ncl_math::Color color)
{
    PlayerTrackPoint world[PlayerTracks::kMaxPoints];
    int count = tracks_.CopyPoints(player, world);
    if (count < 2)
    {
        return;
    }

    ncl_math::Vector2 points[PlayerTracks::kMaxPoints];
    uint8_t alpha[PlayerTracks::kMaxPoints];
    int start = 0;

    for (int k = 0; k < count; k++)
    {
        points[k] = frame.shape.Clamp(1, frame.t.WorldToScreen({world[k].pos.x, world[k].pos.y}));
        alpha[k] = (uint8_t)ncl_math::Lerp(kTrailAlphaTail, kTrailAlphaHead, (float)k / (float)(count - 1));

        if (k > 0 && !RadarLayerMarker_IsTrailSegmentVisible(frame.layer_view, world[k - 1].layer, world[k].layer))
        {
            if (k - start >= 2)
            {
                draw_.DrawPolyline({points + start, (size_t)(k - start)}, {alpha + start, (size_t)(k - start)}, color, kTrackWidth);
            }

            start = k;
        }
    }

    if (count - start >= 2)
    {
        draw_.DrawPolyline({points + start, (size_t)(count - start)}, {alpha + start, (size_t)(count - start)}, color, kTrackWidth);
    }
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
        last_seen_.OnPlayerDeath(player);
    }
}

int PlayersLayer::ComputePlayerNameWidth(int player, const char* name)
{
    NameWidth& e = name_widths_[player];

    if (std::strcmp(e.name, name) != 0)
    {
        V_strcpy_safe(e.name, name);
        
        int width = 0;
        int height = 0;
        gEngfuncs.pfnDrawConsoleStringLen(name, &width, &height);
        
        e.width = width;
    }

    return e.width;
}

void PlayersLayer::DrawHealthBar(ncl_math::Vector2 pos, int dot, int sb_health, float opacity)
{
    float sx = pos.x;
    float sy = pos.y;
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

    // The shader applies the flashbang wash, so the colors are passed through unfaded.
    draw_.DrawRect({(float)(bx - 1), (float)(by - 1)}, {(float)(bx + bw + 1), (float)(by + bh + 1)}, {0, 0, 0, (int)(150 * opacity)});
    draw_.DrawRect({(float)bx, (float)by}, {(float)(bx + bw), (float)(by + bh)}, {30, 30, 30, (int)(200 * opacity)});
    draw_.DrawRect({(float)bx, (float)by}, {(float)(bx + bw * hp / 100), (float)(by + bh)}, hp_col.WithAlpha((int)(255 * opacity)));
}

void PlayersLayer::DrawPlayers(const RadarFrame& frame)
{
    if (!frame.ctx.local)
    {
        return;
    }

    int dot = std::max(2, (int)(frame.box.marker_px * kPlayerDotScale));
    std::array<PlayerJob, kRadarMaxPlayers * 2> jobs;
    bool record_diagnostics = RadarDebug_IsEnabled();
    RadarDrawStats before{};

    if (record_diagnostics)
    {
        before = draw_.GetStats();
    }

    std::span<const PlayerJob> collected = CollectPlayerJobs(frame, dot, jobs, record_diagnostics);
    DrawPlayerLayers(frame, collected, dot, ComputeHighlightConeSpread(frame));

    if (record_diagnostics)
    {
        RadarDrawStats after = draw_.GetStats();
        PlayerMarkerHistory::Frame diagnostic{
            .time = frame.time,
            .message = frame.ctx.message_num,
            .layer = frame.layer_view.active,
            .resets = marker_resets_,
            .jobs = (int)collected.size(),
            .batches = after.batches - before.batches,
            .quads = after.quads - before.quads,
            .broadcast = broadcast_players_,
            .fading = fading_players_,
            .fade_in = fading_in_players_,
            .overlap = overlapping_players_,
            .snapshot_steps = snapshot_step_players_,
        };
        std::array<std::uint32_t, kRadarMaxPlayers> sequences{};

        for (int player = 1; player <= kRadarMaxPlayers; player++)
        {
            sequences[player - 1] = broadcast_.get_player(player).seq;
            if (!drawn_[player])
            {
                continue;
            }
            std::uint64_t bit = std::uint64_t{1} << (player - 1);
            diagnostic.drawn |= bit;
            if (prev_snapshot_[player])
            {
                diagnostic.snapshot |= bit;
            }
        }
        marker_history_.Record(diagnostic, sequences);
    }
}

float PlayersLayer::ComputeHighlightConeSpread(const RadarFrame& frame)
{
    float fov = 90.0f;

    if (gHUD && gHUD->m_iFOV && *gHUD->m_iFOV > 1)
    {
        fov = (float)*gHUD->m_iFOV;
    }

    if (frame.ctx.spectating && !frame.ctx.free_camera && gHUD && gHUD->m_pPlayerFOV && frame.ctx.observed_idx >= 1 &&
        frame.ctx.observed_idx < 64)
    {
        int pf = (*gHUD->m_pPlayerFOV)[frame.ctx.observed_idx];

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
    bool live = ent && ent->player && RadarFrame_IsEntityCurrent(frame.ctx, ent);
    bool is_ally = is_self || (team == frame.ctx.highlight_team && frame.ctx.highlight_team != RadarTeam::Unassigned);

    if (is_ally && !is_self && !policy_.IsAllyRevealed(i))
    {
        is_ally = false;
    }

    ncl_math::Vector3 origin{};
    float facing_yaw = 0.0f;
    bool has_dir = false;
    bool from_broadcast = false;

    if (is_self)
    {
        origin = ncl_math::Vector3::Ref(local->origin);
        facing_yaw = g_MainViewAngles[1];
        has_dir = true;
    }
    else if (is_ally && !frame.ctx.spectating)
    {
        if (live)
        {
            origin = RadarFrame_GetPlayerPosition(*ent);
            facing_yaw = ent->angles[1];
            has_dir = true;
        }
        else
        {
            const RadarBroadcast::Player& report = broadcast_.get_player(i);
            origin = report.seq != 0 ? report.pos : ncl_math::Vector3::Ref(ext.origin);
            if (!IsKnownOrigin(origin))
            {
                return false;
            }
            from_broadcast = true;
        }
    }
    else if (visibility_.is_gating())
    {
        ncl_math::Vector3 p;
        float yaw = 0.0f;

        if (!visibility_.IsEnemyVisible(i, p, yaw))
        {
            return false;
        }

        origin = p;
        facing_yaw = yaw;
        has_dir = true;
    }
    else if (live)
    {
        origin = RadarFrame_GetPlayerPosition(*ent);
        facing_yaw = ent->angles[1];
        has_dir = true;
    }
    else
    {
        return false;
    }

    out.pos = origin;
    out.yaw = facing_yaw;
    out.has_dir = has_dir;
    out.snapshot = is_self || live;
    out.from_broadcast = from_broadcast;
    return true;
}

bool PlayersLayer::IsTeleported(int player, ncl_math::Vector3 pos, float run_speed, float dt) const
{
    if (!prev_alive_[player])
    {
        return false;
    }

    float frame_move = run_speed * std::clamp(dt, 0.005f, 0.1f) + kTeleportPad;

    return (pos - prev_snapshot_pos_[player]).LengthSqr() > frame_move * frame_move;
}

void PlayersLayer::UpdatePlayerTrail(
    const RadarFrame& frame,
    int player,
    const goldsrc::cl_entity_t* ent,
    ncl_math::Vector3 world,
    ncl_math::Color color,
    bool offscreen,
    bool teleported,
    bool break_on_jump
)
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

    if (tracks_.is_sample_due(player, frame.time))
    {
        int layer = -1;
        if (frame.layer_view.enabled && frame.layer_view.map)
        {
            layer = frame.layer_view.map->Locate(world, RadarLayerQuery::Below, frame.layer_view.active).layer;
        }
        tracks_.Sample(player, world, layer, frame.time, break_on_jump);
    }
    DrawTrack(frame, player, color);
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

    if (!frame.ctx.spectating && player == frame.ctx.highlight_idx)
    {
        col = RadarPalette::kHighlight;
    }

    RadarLayerStyle style = RadarLayerMarker_ComputeStyle(frame.layer_view, spot.pos);
    ncl_math::Vector2 marker_raw = frame.t.WorldToScreen({spot.pos.x, spot.pos.y});
    int extent = frame.layer_view.enabled ? (int)std::ceil(dot * 2.4f) : dot;
    ncl_math::Vector2 dir = spot.has_dir ? frame.t.MarkerDirection(spot.yaw) : ncl_math::Vector2{};
    
    RadarPlayerPlacement player_placement = RadarLayerMarker_PlacePlayer(
        frame.shape,
        marker_raw,
        dir,
        dot,
        extent,
        style
    );
    
    const RadarLayerPlacement& placement = player_placement.placement;
    bool carrier = team == RadarTeam::Terrorist && ext.has_c4 && !placement.offscreen;
    RadarTeam my_team = frame.ctx.highlight_team;

    job.pos = placement.marker;
    job.indicator = placement.indicator;
    job.vertical = style.vertical;
    job.dir = dir;
    job.out_dir = player_placement.out_dir;
    job.col = col;
    job.alpha = (int)(255 * style.opacity);
    job.base_alpha = job.alpha;
    job.fade_weight = 1.0f;
    job.cone = (player == frame.ctx.highlight_idx);
    job.carrier = carrier;
    job.offscreen = placement.offscreen;
    // sb_health is broadcast to teammates by the ReGameDLL HealthInfo message
    // (mp_scoreboard_showhealth), unlike .health which only refreshes while aiming at one.
    job.health = !placement.offscreen && g_RadarCfg.health->value != 0.0f && player != frame.ctx.highlight_idx &&
                 my_team != RadarTeam::Unassigned && team == my_team && policy_.IsAllyRevealed(player) && ext.sb_health > 0;
    job.sb_health = ext.sb_health;
    job.name = !placement.offscreen && g_RadarCfg.names->value != 0.0f;
    job.index = player;
    job.is_self = is_self;
}

std::span<const PlayersLayer::PlayerJob> PlayersLayer::CollectPlayerJobs(
    const RadarFrame& frame,
    int dot,
    std::span<PlayerJob> jobs,
    bool record_diagnostics
)
{
    int job_count = 0;
    PlayerLastSeen::Observation visible[kRadarMaxPlayers];
    int visible_count = 0;
    float move_tolerance = dot * 2.0f / frame.t.projection.ApplyVector({frame.t.k, 0.0f}).Length();

    int max_clients = std::min<int>(kRadarMaxPlayers, gEngfuncs.GetMaxClients());

    // The live limit, so the teleport heuristic holds on a modified sv_maxspeed.
    float run_speed = (pmove && pmove->movevars) ? pmove->movevars->maxspeed : kRunSpeedFallback;

    for (int i = 1; i <= kRadarMaxPlayers; i++)
    {
        goldsrc::hud_player_info_t info{};
        if (i <= max_clients)
        {
            gEngfuncs.pfnGetPlayerInfo(i, &info);
        }

        if (!info.name || !info.name[0])
        {
            tracks_.Clear(i);
            prev_alive_[i] = false;
            prev_snapshot_[i] = false;
            last_seen_.Forget(i);
            marker_fade_[i].Reset();
            continue;
        }

        goldsrc::cstrike::extra_player_info_t& ext = client()->g_PlayerExtraInfo[i];

        if (ext.dead)
        {
            RecordPlayerDeathIfNew(i, RadarTeam_From(ext.teamnumber), frame.time);
            tracks_.Clear(i);
            prev_snapshot_[i] = false;
            last_seen_.Forget(i);
            marker_fade_[i].Reset();
            continue;
        }

        RadarTeam team = RadarTeam_From(ext.teamnumber);
        bool is_self = (i == frame.ctx.self_idx && !frame.ctx.spectating);

        if (team != RadarTeam::CounterTerrorist && team != RadarTeam::Terrorist && !is_self)
        {
            tracks_.Clear(i);
            prev_alive_[i] = false;
            prev_snapshot_[i] = false;
            last_seen_.Forget(i);
            marker_fade_[i].Reset();
            continue;
        }

        goldsrc::cl_entity_t* ent = gEngfuncs.GetEntityByIndex(i);
        PlayerSpot spot;

        if (!ResolvePlayerSpot(frame, i, is_self, ent, spot))
        {
            marker_fade_[i].Lose();
            prev_snapshot_[i] = false;
            if (team == frame.ctx.highlight_team && i != frame.ctx.highlight_idx && g_RadarCfg.tracks->value != 0.0f &&
                !tracks_.IsSuppressed(frame.time))
            {
                DrawTrack(frame, i, team == RadarTeam::Terrorist ? RadarPalette::kTerrorist : RadarPalette::kCounterTerrorist);
            }
            continue;
        }

        PlayerMarkerFade::Visible marker{spot.pos};
        if (!is_self)
        {
            marker = marker_fade_[i].Update(spot.pos, frame.time, !spot.from_broadcast, broadcast_.get_player(i).seq, move_tolerance);
            spot.pos = marker.pos;
        }

        std::uint64_t bit = 0;

        if (record_diagnostics)
        {
            bit = std::uint64_t{1} << (i - 1);

            if (spot.from_broadcast)
            {
                broadcast_players_ |= bit;
            }

            if (!is_self)
            {
                if (marker.alpha < 1.0f || marker.previous_alpha > 0.0f)
                {
                    fading_players_ |= bit;
                }

                if (marker.alpha < 1.0f && marker.previous_alpha == 0.0f)
                {
                    fading_in_players_ |= bit;
                }

                ncl_math::Vector2 move{spot.pos.x - prev_pos_[i].x, spot.pos.y - prev_pos_[i].y};

                if (spot.snapshot && prev_snapshot_[i] && marker.alpha == 1.0f && marker.previous_alpha == 0.0f &&
                    move.LengthSqr() > move_tolerance * move_tolerance)
                {
                    snapshot_step_players_ |= bit;
                }
            }
        }

        if (!is_self)
        {
            visible[visible_count++] = {i, spot.pos, team, spot.yaw, spot.has_dir, team == RadarTeam::Terrorist && ext.has_c4};
        }

        bool teleported = false;
        if (spot.snapshot)
        {
            ncl_math::Vector3 snapshot_pos = is_self ? ncl_math::Vector3::Ref(frame.ctx.local->origin) : RadarFrame_GetPlayerPosition(*ent);
            teleported = prev_snapshot_[i] && IsTeleported(i, snapshot_pos, run_speed, frame.dt);
            prev_snapshot_pos_[i] = snapshot_pos;
        }

        bool break_on_jump = spot.snapshot && prev_snapshot_[i];
        prev_snapshot_[i] = spot.snapshot;
        prev_alive_[i] = true;
        prev_pos_[i] = spot.pos;
        drawn_[i] = true;

        PlayerJob& job = jobs[job_count++];
        FillPlayerJob(frame, i, is_self, team, spot, ext, dot, job);
        job.alpha = (int)(job.alpha * marker.alpha);
        job.fade_weight = marker.alpha;

        UpdatePlayerTrail(frame, i, spot.snapshot ? ent : nullptr, spot.pos, job.col, job.offscreen, teleported, break_on_jump);
        if (marker.previous_alpha > 0.0f)
        {
            PlayerSpot previous = spot;
            previous.pos = marker.previous_pos;
            PlayerJob& previous_job = jobs[job_count++];
            FillPlayerJob(frame, i, is_self, team, previous, ext, dot, previous_job);
            previous_job.alpha = (int)(previous_job.alpha * marker.previous_alpha);
            previous_job.fade_weight = marker.previous_alpha;

            if (record_diagnostics && (job.pos - previous_job.pos).LengthSqr() < dot * dot * 4.0f)
            {
                overlapping_players_ |= bit;
            }
        }
    }

    last_seen_.Update({visible, (size_t)visible_count}, frame.time);

    return jobs.first(job_count);
}

void PlayersLayer::DrawPlayerLayers(const RadarFrame& frame, std::span<const PlayerJob> jobs, int dot, float cone_spread)
{
    draw_.BeginPrims();
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
        draw_.DrawCone(job.pos, fwd, dot * 6.5f, cone_spread, light, 0.65f * (job.alpha / 255.0f));
    }
    draw_.EndPrims();

    draw_.BeginPrims();
    for (size_t index = 0; index < jobs.size(); index++)
    {
        size_t first = index;
        const PlayerJob& job = jobs[index];
        
        if (index + 1 < jobs.size() && jobs[index + 1].index == job.index)
        {
            const PlayerJob& previous = jobs[++index];
            
            RadarDrawPlayerMarker current_marker{
                job.pos, job.offscreen ? job.out_dir : job.dir, job.base_alpha / 255.0f, job.fade_weight, job.offscreen
            };
            
            RadarDrawPlayerMarker previous_marker{
                previous.pos,
                previous.offscreen ? previous.out_dir : previous.dir,
                previous.base_alpha / 255.0f,
                previous.fade_weight,
                previous.offscreen,
            };

            draw_.DrawPlayerMarkerFade(current_marker, previous_marker, (float)dot, job.col);
        }
        else if (job.offscreen)
        {
            DrawEdgeArrow(job.pos, job.out_dir, dot, job.col.WithAlpha(job.alpha));
        }
        else
        {
            DrawMarker(job.pos, job.dir, dot, job.col.WithAlpha(job.alpha));
        }
        for (size_t carrier_index = first; carrier_index <= index; carrier_index++)
        {
            const PlayerJob& carrier = jobs[carrier_index];
            
            if (carrier.carrier)
            {
                draw_.DrawRing(
                    carrier.pos, (float)dot, dot * (1.0f - kCarrierRingWidthScale), RadarPalette::kGold.WithAlpha(carrier.alpha)
                );
            }
        }
    }

    for (const PlayerJob& job : jobs)
    {
        draw_.DrawLayerIndicator(job.indicator, job.vertical, dot, job.col.WithAlpha(job.alpha));
    }

    for (const PlayerJob& job : jobs)
    {
        if (job.health)
        {
            DrawHealthBar(job.vertical > 0 ? job.indicator : job.pos, dot, job.sb_health, job.alpha / 255.0f);
        }
    }
    draw_.EndPrims();

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
        draw_.BeginPrims();
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
            RadarLabel_Draw(
                draw_, name, (int)job.pos.x - tw / 2, (int)job.pos.y + dot + 1, job.col.WithAlpha(job.alpha), frame.flash_alpha
            );
        }
    }

    if (batched)
    {
        draw_.EndPrims();
    }
}

void PlayersLayer::DrawLastSeen(const RadarFrame& frame)
{
    if (last_seen_.get_marks().empty())
    {
        return;
    }

    float sz = std::max(kMemoryMarkMinSize, frame.box.marker_px * kMemoryMarkScale);
    float hw = std::max(kMemoryMarkMinHalfWidth, sz * kMemoryMarkWidthScale) * kQuestionMarkWidthFactor;
    int question_inset = (int)std::ceil(sz + hw);
    int dot = std::max(2, (int)(frame.box.marker_px * kPlayerDotScale));
    int marker_inset = frame.layer_view.enabled ? (int)std::ceil(dot * 2.4f) : dot;

    draw_.BeginPrims();

    for (const PlayerLastSeen::Mark& mark : last_seen_.get_marks())
    {
        RadarLayerStyle style = RadarLayerMarker_ComputeStyle(frame.layer_view, mark.pos);
        int inset = mark.uncertain ? question_inset : marker_inset;
        int indicator_size = mark.uncertain ? inset : dot;
        ncl_math::Vector2 raw = frame.t.WorldToScreen({mark.pos.x, mark.pos.y});
        ncl_math::Vector2 dir = mark.has_dir ? frame.t.MarkerDirection(mark.yaw) : ncl_math::Vector2{};
        std::optional<float> half_height;

        if (mark.uncertain)
        {
            half_height = sz + hw;
        }

        RadarPlayerPlacement player_placement = RadarLayerMarker_PlacePlayer(
            frame.shape,
            raw,
            dir,
            dot,
            inset,
            style,
            indicator_size,
            half_height
        );
        
        const RadarLayerPlacement& placement = player_placement.placement;
        ncl_math::Vector2 pos = placement.marker;
        ncl_math::Color color = mark.team == RadarTeam::Terrorist ? RadarPalette::kTerrorist : RadarPalette::kCounterTerrorist;
        color = color.WithAlpha((int)(mark.alpha * style.opacity));

        if (mark.uncertain)
        {
            draw_.DrawQuestionMark(pos, sz, hw, color);
        }
        else if (placement.offscreen)
        {
            DrawEdgeArrow(pos, player_placement.out_dir, dot, color);
        }
        else
        {
            DrawMarker(pos, dir, dot, color);

            if (mark.carrier)
            {
                draw_.DrawRing(pos, (float)dot, dot * (1.0f - kCarrierRingWidthScale), RadarPalette::kGold.WithAlpha(color.a));
            }
        }
        draw_.DrawLayerIndicator(placement.indicator, style.vertical, indicator_size, color);
    }

    draw_.EndPrims();
}

void PlayersLayer::DrawDeaths(const RadarFrame& frame)
{
    DeathMarks::Visible marks[DeathMarks::kMax];
    int n = deaths_.CollectVisible(marks, frame.time);
    float sz = std::max(kMemoryMarkMinSize, frame.box.marker_px * kMemoryMarkScale);
    float hw = std::max(kMemoryMarkMinHalfWidth, sz * kMemoryMarkWidthScale);
    int inset = (int)std::ceil(sz * std::sqrt(2.0f) + hw);

    draw_.BeginPrims();

    for (int i = 0; i < n; i++)
    {
        RadarLayerStyle style = RadarLayerMarker_ComputeStyle(frame.layer_view, marks[i].pos);
        RadarLayerPlacement placement =
            RadarLayerMarker_Place(frame.shape, frame.t.WorldToScreen({marks[i].pos.x, marks[i].pos.y}), inset, style, inset, sz + hw);
        ncl_math::Vector2 s = placement.marker;

        ncl_math::Color color = marks[i].color.WithAlpha((int)(marks[i].alpha * style.opacity));

        draw_.DrawCross(s, sz, hw, color);
        draw_.DrawLayerIndicator(placement.indicator, style.vertical, inset, color);
    }

    draw_.EndPrims();
}
