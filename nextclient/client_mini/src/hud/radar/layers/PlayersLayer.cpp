#include "PlayersLayer.h"

#include "../RadarConfig.h"
#include "../RadarDraw.h"
#include "../RadarLabel.h"
#include "../RadarPalette.h"
#include "../RadarPolicy.h"
#include "../RadarText.h"
#include "../Visibility.h"
#include "../RadarDump.h"
#include "../../../main.h"
#include "../../../view.h"

#include <strtools.h>
#include <ncl_math/vec3.h>
#include <next_client_mini/radar_cvars.h>
#include <pm_defs.h>
#include <pm_movevars.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace
{
    // Marker sizes as a fraction of box.elem (the resolution-damped box size).
    constexpr float kPlayerDotScale = 0.023f;
    constexpr float kDeathCrossScale = 0.02f;
    constexpr float kTrackWidth = 2.0f;

    // Teleport detection.
    constexpr float kRunSpeedFallback = 320.0f; // stock sv_maxspeed, used until movevars are up
    constexpr float kTeleportPad = 24.0f; // slack added to the per-frame move budget
    constexpr float kTrailAlphaTail = 30.0f; // trail alpha gradient: oldest sample...
    constexpr float kTrailAlphaHead = 200.0f; // ...toward the newest

    // Nicknames render a touch smaller than the standard HUD text, then scale with the size cvar.
    constexpr float kNameScale = 0.85f;
    // Fallback nickname height (fraction of box.elem) used only until the scheme font resolves.
    constexpr float kNameHeightScale = 0.055f;
    constexpr int kNameMinPx = 9;
    constexpr int kNameMaxPx = 28;

    // Longest nickname drawn on the radar, counted in Unicode codepoints; longer names are cut.
    constexpr int kMaxNameChars = 16;

    // Cvar value ranges shared with the settings dialog (see radar_cvars.h).
    constexpr RadarCvarDef kHealthSizeCvar = RadarCvarDef_Get("hud_radar_health_size");
    static_assert(kHealthSizeCvar.name != nullptr);

    constexpr RadarCvarDef kNameSizeCvar = RadarCvarDef_Get("hud_radar_name_size");
    static_assert(kNameSizeCvar.name != nullptr);

    bool IsKnownOrigin(const float* o)
    {
        return o[0] != 0.0f || o[1] != 0.0f || o[2] != 0.0f;
    }

    // Copies at most max_cp UTF-8 codepoints of `in` into `out` (NUL-terminated). Counts whole
    // codepoints from their lead byte, so a multibyte nickname is never cut mid-character.
    void TruncateUtf8(const char* in, int max_cp, char* out, size_t out_size)
    {
        const uint8_t* s = reinterpret_cast<const uint8_t*>(in);
        size_t oi = 0;
        int cp = 0;

        while (*s != 0 && cp < max_cp)
        {
            int len = 1;

            if (*s >= 0xF0)
            {
                len = 4;
            }
            else if (*s >= 0xE0)
            {
                len = 3;
            }
            else if (*s >= 0xC0)
            {
                len = 2;
            }

            // A truncated trailing sequence stops the copy rather than reading past the string.
            int actual = 0;
            while (actual < len && s[actual] != 0)
            {
                actual++;
            }

            if (oi + (size_t)actual + 1 > out_size)
            {
                break;
            }

            for (int k = 0; k < actual; k++)
            {
                out[oi++] = (char)s[k];
            }

            s += actual;
            cp++;
        }

        out[oi] = '\0';
    }
} // namespace

PlayersLayer::PlayersLayer(RadarVisibility& visibility, const RadarPolicy& policy) :
    visibility_(visibility),
    policy_(policy)
{
    static_assert(kMaxPlayers == MAX_PLAYERS, "player table size mismatch");
}

void PlayersLayer::BeginFrame(float time)
{
    tracks_.BeginFrame(time);
}

void PlayersLayer::ResetRound()
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
    // The FOV cone is drawn by its own pass in DrawPlayerLayers, so this only lays
    // down the dot and its arrowhead; a zero dir draws the dot alone.
    int ai = std::clamp((int)color.a, 0, 255);

    float dir_len = dir.Length();
    bool is_dir = dir_len > ncl_math::kNearZero;
    ncl_math::Vector2 fwd = is_dir ? dir / dir_len : ncl_math::Vector2{};

    if (is_dir)
    {
        // White arrowhead growing out of the dot. Both base corners sit on the dot's
        // rim (or the carrier halo's rim), so the head flows out of the circle and
        // never juts out the side or back; the tip reaches a bit further out. The
        // base half-angle sets how blunt the head is.
        float cut = std::max((float)size, halo_radius);
        float spread = 0.62f; // half-angle of the base on the rim
        float tip_d = cut + size * 1.0f;
        ncl_math::Vector2 tip = pos + fwd * tip_d;
        ncl_math::Vector2 base_l = pos + fwd.Rotated(spread) * cut;
        ncl_math::Vector2 base_r = pos + fwd.Rotated(-spread) * cut;
        RadarDraw_DrawTriangle(tip, base_l, base_r, {240, 240, 240, ai});
    }

    RadarDraw_DrawDisc(pos, (float)size, color.WithAlpha(ai));
}

void PlayersLayer::DrawEdgeArrow(ncl_math::Vector2 pos, ncl_math::Vector2 out_dir, int size, ncl_math::Color color)
{
    float len = out_dir.Length();

    // A degenerate heading (player projected onto the exact centre) cannot happen off the rim, but
    // fall back to the plain dot rather than emit a zero-area triangle.
    if (len <= ncl_math::kNearZero)
    {
        RadarDraw_DrawDisc(pos, (float)size, color);
        return;
    }

    ncl_math::Vector2 fwd = out_dir / len;
    ncl_math::Vector2 perp = fwd.Perp();

    // Isosceles arrowhead pointing outward: the tip reaches toward the rim, the base sits behind
    // the clamped position, sized off the dot so it reads at the same scale as the markers.
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
        pts[k] = RadarView_ClampToRadar(frame.box, frame.round, 1, frame.t.WorldToScreen(world[k]));
        al[k] = (uint8_t)ncl_math::Lerp(kTrailAlphaTail, kTrailAlphaHead, (float)k / (float)std::max(1, count));
    }

    // Record the trail head (newest sample) so it is covered by radar_dump.
    RadarDump::AddMarker(
        "trail", player, world[count - 1].x, world[count - 1].y, pts[count - 1].x, pts[count - 1].y, color.r, color.g, color.b,
        al[count - 1], ""
    );

    RadarDraw_DrawPolyline(pts, al, count, color, kTrackWidth);
}

void PlayersLayer::RecordPlayerDeathIfNew(int player, int teamnumber, float time)
{
    if (!prev_alive_[player])
    {
        return;
    }

    prev_alive_[player] = false;

    if (teamnumber == TEAM_TERRORIST || teamnumber == TEAM_CT)
    {
        const ncl_math::Color& dc = (teamnumber == TEAM_TERRORIST) ? RadarPalette::kTerrorist : RadarPalette::kCounterTerrorist;
        deaths_.Add(prev_pos_[player], dc, time);
    }
}

int PlayersLayer::PlayerNameWidth(int player, const char* name)
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
    RadarDraw_DrawRect({(float)bx, (float)by}, {(float)(bx + bw * hp / 100), (float)(by + bh)}, hp_col.WithAlpha(255));
}

void PlayersLayer::DrawPlayers(const RadarFrame& frame)
{
    if (!frame.ctx.local)
    {
        return;
    }

    int dot = std::max(2, (int)(frame.box.elem * kPlayerDotScale));
    std::array<PlayerJob, kMaxPlayers + 1> jobs;
    int njobs = CollectPlayerJobs(frame, dot, jobs.data());
    DrawPlayerLayers(frame, jobs.data(), njobs, dot, HighlightConeSpread(frame));
}

float PlayersLayer::HighlightConeSpread(const RadarFrame& frame)
{
    // Cone half-angle for the highlighted player from its current FOV.
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

// Decides where player i is drawn from this frame: the view origin for the local
// player, the live entity or the radar broadcast for allies, the line-of-sight spot
// memory for enemies, the PVS for spectators. False = not drawn (the caller drops
// the trail).
bool PlayersLayer::ResolvePlayerSpot(const RadarFrame& frame, int i, bool is_self, cl_entity_t* ent, PlayerSpot* out)
{
    cl_entity_t* local = frame.ctx.local;
    extra_player_info_t& ext = client()->g_PlayerExtraInfo[i];
    int team = ext.teamnumber;
    bool live = ent && ent->player && ent->curstate.messagenum == local->curstate.messagenum;
    bool is_ally = is_self || (team == frame.ctx.highlight_team && frame.ctx.highlight_team != 0);

    // Sharing a team is the client's notion of an ally; the right to draw one without
    // the line-of-sight gate is the server's to give, and a free-for-all server takes
    // it back by withholding the radar broadcast. Without it the teammate is resolved
    // like an enemy (see RadarPolicy).
    if (is_ally && !is_self && !policy_.AllyRevealed(i))
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
            origin = ext.origin; // teammate radar broadcast (current map only)
        }
    }
    else if (visibility_.gating())
    {
        // Enemy: reveal only while the team's line-of-sight memory is fresh,
        // fading the marker out over the tail of that window. Always use the
        // spotted position/facing (updated only while actually seen) so the
        // marker freezes the instant sight is lost instead of tracking live.
        ncl_math::Vector2 p;
        float a = 1.0f, yaw = 0.0f;

        if (!visibility_.ShouldShowEnemy(i, frame.time, &p, &yaw, &a))
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
        // Spectator or LOS disabled: show whoever the PVS provides.
        origin = ent->origin;
        facing_yaw = ent->angles[1];
        has_dir = true;
    }
    else
    {
        return false;
    }

    if (!IsKnownOrigin(origin))
    {
        return false;
    }

    out->pos = {origin[0], origin[1]};
    out->yaw = facing_yaw;
    out->alpha = marker_alpha;
    out->has_dir = has_dir;
    return true;
}

int PlayersLayer::CollectPlayerJobs(const RadarFrame& frame, int dot, PlayerJob* jobs)
{
    // Trails draw here as their players are resolved, so they land under every marker
    // layer; the markers themselves are deferred into jobs for the layered passes.
    int my_team = frame.ctx.highlight_team;
    int njobs = 0;

    // The engine caps maxclients at 32; the client arrays are sized for 64, but the
    // slots past maxclients are always empty, so do not walk them.
    int max_clients = std::min<int>(kMaxPlayers, gEngfuncs.GetMaxClients());

    // Live server speed limit, so the teleport heuristic stays valid on servers
    // with a modified sv_maxspeed.
    float run_speed = (pmove && pmove->movevars) ? pmove->movevars->maxspeed : kRunSpeedFallback;

    for (int i = 1; i <= max_clients; i++)
    {
        extra_player_info_t& ext = client()->g_PlayerExtraInfo[i];

        if (ext.dead)
        {
            RecordPlayerDeathIfNew(i, ext.teamnumber, frame.time);
            tracks_.Clear(i);
            continue;
        }

        int team = ext.teamnumber;
        bool is_self = (i == frame.ctx.self_idx && !frame.ctx.spectating);

        if (team != TEAM_CT && team != TEAM_TERRORIST && !is_self)
        {
            continue;
        }

        cl_entity_t* ent = gEngfuncs.GetEntityByIndex(i);
        PlayerSpot spot;

        if (!ResolvePlayerSpot(frame, i, is_self, ent, &spot))
        {
            // Keep the trail across a transient PVS gap so it resumes on return instead of restarting;
            // the trail's own break distance handles a far re-acquire.
            continue;
        }

        // Detect a teleport/respawn by a single-frame jump larger than running
        // could cover that frame. This catches a round restart onto a nearby
        // spawn, where the move is too small for the trail's per-sample check.
        bool teleported = false;

        if (prev_alive_[i])
        {
            float frame_move = run_speed * std::clamp(frame.dt, 0.005f, 0.1f) + kTeleportPad;
            ncl_math::Vector2 jump = spot.pos - prev_pos_[i];
            teleported = jump.LengthSqr() > frame_move * frame_move;
        }

        prev_alive_[i] = true;
        prev_pos_[i] = spot.pos;

        // The scoreboard VIP bit is server-controlled and routinely re-purposed to mark admins, so it does not tint the marker.
        ncl_math::Color col = team == TEAM_TERRORIST ? RadarPalette::kTerrorist : RadarPalette::kCounterTerrorist;

        // The controlled/observed player is shown green to stand out from both teams.
        if (i == frame.ctx.highlight_idx)
        {
            col = RadarPalette::kHighlight;
        }

        ncl_math::Vector2 marker_raw = frame.t.WorldToScreen(spot.pos);

        ncl_math::Vector2 dir{};

        if (spot.has_dir)
        {
            dir = frame.t.MarkerDirection(spot.yaw);
        }

        ncl_math::Vector2 marker = RadarView_ClampToRadar(frame.box, frame.round, dot, marker_raw);

        // Off the radar only when the projected position leaves the radar shape itself, tested against
        // the true rim (box.size*0.5) rather than the dot-inset clamp above, so a player still inside
        // the disc but within a dot's width of the border keeps their dot/name/health. Such a player is
        // drawn as an arrow pointing their way, with no name/health/halo (see the draw passes), and
        // out_dir carries the radial heading from the centre toward the real position.
        ncl_math::Vector2 off = marker_raw - frame.t.center;
        float off_len = off.Length();
        float half = frame.box.size * 0.5f;
        bool offscreen = frame.round
            ? off.LengthSqr() > half * half
            : (std::abs(off.x) > half || std::abs(off.y) > half);
        ncl_math::Vector2 out_dir = (offscreen && off_len > ncl_math::kNearZero) ? off / off_len : ncl_math::Vector2{};

        // No trail for the controlled/observed player (it clutters the centred marker) nor for an
        // offscreen player (its samples would all clamp onto the rim as a squiggle under the arrow);
        // the trail is left intact across the gap so it resumes on return.
        if (g_RadarCfg.tracks->value != 0.0f && i != frame.ctx.highlight_idx && !offscreen)
        {
            if (teleported || (ent && (ent->curstate.effects & EF_NOINTERP)))
            {
                tracks_.Clear(i);
            }

            if (!tracks_.Suppressed(frame.time))
            {
                DrawTrack(frame, i, spot.pos, col);
            }
            else
            {
                tracks_.Clear(i);
            }
        }
        else if (i == frame.ctx.highlight_idx)
        {
            tracks_.Clear(i);
        }

        bool carrier = team == TEAM_TERRORIST && ext.has_c4 && !offscreen;

        PlayerJob& job = jobs[njobs++];
        job.pos = marker;
        job.dir = dir;
        job.out_dir = out_dir;
        job.col = col;
        job.alpha = spot.alpha;
        job.cone = (i == frame.ctx.highlight_idx);
        job.carrier = carrier;
        job.halo = carrier ? dot * 1.4f : 0.0f; // ring hugs the dot edge
        job.offscreen = offscreen;
        // sb_health is broadcast to teammates via the ReGameDLL HealthInfo message
        // (mp_scoreboard_showhealth, default 3) on every damage event and every 5s,
        // unlike .health which only refreshes when aiming at a teammate. A teammate the
        // server no longer reveals is an opponent, so its health is withheld too.
        job.health = !offscreen && g_RadarCfg.health->value != 0.0f && i != frame.ctx.highlight_idx && my_team != 0 &&
            team == my_team && policy_.AllyRevealed(i) && ext.sb_health > 0;
        job.sb_health = ext.sb_health;
        job.name = !offscreen && g_RadarCfg.names->value != 0.0f;
        job.index = i;

        RadarDump::AddMarker(
            "player", i, spot.pos.x, spot.pos.y, marker.x, marker.y, col.r, col.g, col.b, spot.alpha,
            i == frame.ctx.highlight_idx ? "highlight" : ""
        );
    }

    return njobs;
}

void PlayersLayer::DrawPlayerLayers(const RadarFrame& frame, const PlayerJob* jobs, int njobs, int dot, float cone_spread)
{
    // FOV cones (additive wash), batched together. Drawn under the dots so a cone
    // never brightens another player's marker sitting inside its spread.
    RadarDraw_BeginPrims();

    for (int j = 0; j < njobs; j++)
    {
        const PlayerJob& job = jobs[j];

        if (!job.cone || job.offscreen)
        {
            continue;
        }

        float dir_len = job.dir.Length();

        if (dir_len <= ncl_math::kNearZero)
        {
            continue;
        }

        ncl_math::Vector2 fwd = job.dir / dir_len;
        // Lightened two thirds toward white, matching the old in-marker cone.
        ncl_math::Color light = ncl_math::Lerp(job.col.WithAlpha(job.alpha), ncl_math::Color{255, 255, 255}, 2.0f / 3.0f);
        RadarDraw_DrawCone(job.pos, fwd, dot * 6.5f, cone_spread, light, 0.65f * (job.alpha / 255.0f));
    }

    RadarDraw_EndPrims();

    // The carrier halo, arrowhead and dot of every player, batched together. The
    // arrowhead starts beyond the halo ring so it stays readable over the map.
    RadarDraw_BeginPrims();

    for (int j = 0; j < njobs; j++)
    {
        const PlayerJob& job = jobs[j];

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

    // Health bars share the marker batch now that they are prim quads rather than engine
    // immediate-mode fills; they sit above the dot, so drawing them here keeps them over it.
    for (int j = 0; j < njobs; j++)
    {
        const PlayerJob& job = jobs[j];

        if (job.health)
        {
            DrawHealthBar(job.pos, dot, job.sb_health);
        }
    }

    RadarDraw_EndPrims();

    DrawNames(frame, jobs, njobs, dot);
}

int PlayersLayer::NameHeightPx(const RadarFrame& frame)
{
    float mul = std::clamp(g_RadarCfg.name_size->value, kNameSizeCvar.min_value, kNameSizeCvar.max_value);
    int standard = RadarText_StandardPx();

    // Size relative to the standard HUD text (slightly smaller), scaled by the user's size cvar. Fall
    // back to the box-relative ladder only until the scheme font resolves.
    int px = standard > 0 ? (int)std::lround(standard * kNameScale * mul)
                          : (int)std::lround(frame.box.elem * kNameHeightScale * mul);

    return std::clamp(px, kNameMinPx, kNameMaxPx);
}

void PlayersLayer::DrawNames(const RadarFrame& frame, const PlayerJob* jobs, int njobs, int dot)
{
    bool any_name = false;

    for (int j = 0; j < njobs && !any_name; j++)
    {
        any_name = jobs[j].name;
    }

    // Nothing to draw: return before touching the font, so the GDI face and the atlas are
    // never built for a player who keeps names off.
    if (!any_name)
    {
        return;
    }

    int name_px = NameHeightPx(frame);
    bool batched = RadarText_Available(name_px);

    // The batched glyph fades through the prim batch's per-vertex alpha, so the label can ramp out with
    // the flash instead of the hard cut the console-font fallback forces (its baked black shadow cannot
    // fade). The fallback keeps the hard cut, since it still draws the baked-shadow console font.
    float flash_fade = batched ? RadarLabel_FlashFade(frame.flash_alpha) : (frame.flash_alpha >= kLabelFlashHide ? 0.0f : 1.0f);

    if (flash_fade <= 0.0f)
    {
        return;
    }

    if (batched)
    {
        RadarDraw_BeginPrims();
    }

    for (int j = 0; j < njobs; j++)
    {
        const PlayerJob& job = jobs[j];

        if (!job.name)
        {
            continue;
        }

        hud_player_info_t info;
        std::memset(&info, 0, sizeof(info));
        gEngfuncs.pfnGetPlayerInfo(job.index, &info);

        if (!info.name || !info.name[0])
        {
            continue;
        }

        char name[80];
        TruncateUtf8(info.name, kMaxNameChars, name, sizeof(name));

        if (batched)
        {
            // The shader applies the flashbang colour wash to the glyphs; the flash also ramps the
            // label alpha here so it fades out toward the blinding peak and back in as sight returns.
            int alpha = (int)(job.alpha * flash_fade);
            RadarText_DrawCentered(job.index, name, {job.pos.x, job.pos.y + dot + 1}, name_px, job.col.WithAlpha(alpha));
        }
        else
        {
            int tw = PlayerNameWidth(job.index, name);
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
    int n = deaths_.CollectVisible(marks, DeathMarks::kMax, frame.time);
    float sz = std::max(2.0f, frame.box.elem * kDeathCrossScale);

    RadarDraw_BeginPrims();

    for (int i = 0; i < n; i++)
    {
        ncl_math::Vector2 s = RadarView_ClampToRadar(frame.box, frame.round, (int)sz, frame.t.WorldToScreen(marks[i].pos));

        RadarDump::AddMarker(
            "death", i, marks[i].pos.x, marks[i].pos.y, s.x, s.y, marks[i].color.r, marks[i].color.g, marks[i].color.b,
            marks[i].alpha, ""
        );

        // Line half-width scales with the cross so it does not look thin at high
        // resolutions. One shape, so the centre fades at the same rate as the arms.
        float hw = std::max(1.3f, sz * 0.42f);
        RadarDraw_DrawCross(s, sz, hw, marks[i].color.WithAlpha(marks[i].alpha));
    }

    RadarDraw_EndPrims();
}
