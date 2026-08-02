#include "ObjectivesLayer.h"

#include "../RadarConfig.h"
#include "../RadarDraw.h"
#include "../RadarLabel.h"
#include "../RadarBroadcast.h"
#include "../RadarPalette.h"
#include "../RadarPolicy.h"
#include "../RadarText.h"
#include "../Visibility.h"
#include "../RadarDump.h"
#include "../../../main.h"

#include <ncl_math/scalar.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    // Bomb-site letter height as a fraction of box.elem, reproducing the engine console font's
    // ladder within a pixel at 768 and 1080. Sites get no size cvar: they are two gold glyphs, not
    // ten nicknames, and shrinking them with the names would be wrong.
    constexpr float kSiteHeightScale = 0.093f;
    constexpr int kSiteMinPx = 9;
    constexpr int kSiteMaxPx = 28;

    // Layout cache slots after the player range (see RadarText).
    constexpr int kSiteSlotBase = 70;

    // Marker sizes as a fraction of box.elem (the resolution-damped box size).
    constexpr float kHostageDotScale = 0.021f;
    constexpr float kBombDotScale = 0.0273f;
    constexpr float kBombPulsePeriod = 4.4f; // seconds per planted-bomb radar-ping ring cycle

    constexpr float kHostageMatchDist = 200.0f; // a broadcast hostage this close to a live entity is the same one

    constexpr int kHistoryMask = HISTORY_MAX - 1;

    // Reproduce the engine's CL_InterpolateModel position interpolation from the
    // entity's history so a moving entity (a thrown bomb) reads as smoothly on the
    // radar as it renders in the world; ent->origin alone is the discrete snapshot
    // position and steps at the server update rate.
    bool InterpEntityXY(cl_entity_t* ent, float target, ncl_math::Vector2* out)
    {
        int cur = ent->current_position;
        int idx0 = cur & kHistoryMask;
        int idx1 = (cur - 1) & kHistoryMask;

        if (ent->ph[idx0].animtime >= target)
        {
            for (int i = 1; i < kHistoryMask; i++)
            {
                int p = (cur - i) & kHistoryMask;

                if (ent->ph[p].animtime == 0.0f)
                {
                    break;
                }

                if (ent->ph[p].animtime < target)
                {
                    idx0 = (cur - i + 1) & kHistoryMask;
                    idx1 = (cur - i) & kHistoryMask;
                    break;
                }
            }
        }

        const position_history_t& ph0 = ent->ph[idx0];
        const position_history_t& ph1 = ent->ph[idx1];

        if (ph1.animtime == 0.0f || ph0.animtime <= ph1.animtime)
        {
            return false;
        }

        float scale = std::clamp((target - ph1.animtime) / (ph0.animtime - ph1.animtime), 0.0f, 1.0f);
        ncl_math::Vector2 p0{ph0.origin[0], ph0.origin[1]};
        ncl_math::Vector2 p1{ph1.origin[0], ph1.origin[1]};
        *out = ncl_math::Lerp(p1, p0, scale);
        return true;
    }
} // namespace

ObjectivesLayer::ObjectivesLayer(RadarVisibility& visibility, const RadarPolicy& policy) :
    visibility_(visibility),
    policy_(policy)
{
}

void ObjectivesLayer::ResetRound()
{
    obj_memory_.Reset();
    bomb_pulse_start_ = -1.0f;
    bomb_appear_time_ = -1.0f;
    last_obj_test_ = 0.0f;
    marker_valid_ = false;
}

void ObjectivesLayer::OnVidInit()
{
    model_kind_.clear(); // model pointers are only stable within a level
    // Level change: drop the objectives carried across from the old map. Neither is
    // cleared per round: gmsgResetHUD also fires on a mid-round respawn, and the server
    // only resends a hostage position once it moves, so dropping the table there would
    // leave the radar (and the "does the server reveal hostages" answer) empty for the
    // rest of the round. A genuine round restart refreshes both within a second.
    RadarBroadcast::ClearBomb(gEngfuncs.GetClientTime());
    RadarBroadcast::ResetHostages();
}

const RadarMapObjectives& ObjectivesLayer::map_objectives()
{
    objectives_.EnsureLoaded();
    return objectives_;
}

ObjectivesLayer::EntModelKind ObjectivesLayer::ClassifyModel(const model_t* model)
{
    auto it = model_kind_.find(model);

    if (it != model_kind_.end())
    {
        return it->second;
    }

    // Substring, not exact match, so custom hostage/bomb model paths still classify.
    EntModelKind kind = EntModelKind::Other;

    if (std::strstr(model->name, "hostage"))
    {
        kind = EntModelKind::Hostage;
    }
    else if (std::strstr(model->name, "backpack"))
    {
        kind = EntModelKind::BombDropped;
    }
    else if (std::strstr(model->name, "w_c4"))
    {
        kind = EntModelKind::BombPlanted;
    }

    model_kind_.emplace(model, kind);
    return kind;
}

void ObjectivesLayer::CollectEntities(const RadarFrame& frame)
{
    // Cadence for the throttled hostage line-of-sight tests (positions still track
    // every frame; see DrawHostages).
    obj_test_due_ = (frame.time - last_obj_test_ >= kObjTestInterval) || frame.time < last_obj_test_;

    if (obj_test_due_)
    {
        last_obj_test_ = frame.time;
    }

    frame_ents_ = FrameEntities{};

    // Every consumer (the bomb and hostage draws) is an objective marker.
    if (g_RadarCfg.objectives->value == 0.0f)
    {
        return;
    }

    cl_entity_t* local = frame.ctx.local;

    if (!local)
    {
        return;
    }

    int message_num = local->curstate.messagenum;

    // Classify one edict and fold it into frame_ents_. Returns true when it is an
    // objective entity worth caching (a hostage or a bomb world model).
    auto consider = [&](int i) {
        cl_entity_t* ent = gEngfuncs.GetEntityByIndex(i);

        if (!ent || !ent->model || ent->curstate.messagenum != message_num)
        {
            return false;
        }

        EntModelKind kind = ClassifyModel(ent->model);

        if (kind == EntModelKind::Other)
        {
            return false;
        }

        if (kind == EntModelKind::Hostage)
        {
            if (frame_ents_.hostage_count < FrameEntities::kMaxHostages)
            {
                FrameEntities::HostageEnt& h = frame_ents_.hostages[frame_ents_.hostage_count++];
                h.index = i;
                h.origin = ncl_math::Vector3::Ref(ent->origin);
            }
        }
        // First bomb world model wins.
        else if (!frame_ents_.bomb.found)
        {
            frame_ents_.bomb.found = true;
            frame_ents_.bomb.ent = ent;
            frame_ents_.bomb.planted = (kind == EntModelKind::BombPlanted);
            frame_ents_.bomb.pos = {ent->origin[0], ent->origin[1]};
        }

        return true;
    };

    if (obj_test_due_)
    {
        // Throttled full rescan to rediscover the objective indices. The engine hands
        // world entities indices from GetMaxClients()+1, so on a server with fewer
        // slots than MAX_PLAYERS the bomb/hostage entities land at or below MAX_PLAYERS
        // and a MAX_PLAYERS+1 lower bound would never find them. The upper bound is
        // probed via GetEntityByIndex (null past the live edict table): the real limit
        // comes from -num_edicts and exceeds the SDK MAX_EDICTS constant.
        objective_ents_.clear();

        for (int i = gEngfuncs.GetMaxClients() + 1; gEngfuncs.GetEntityByIndex(i) != nullptr; i++)
        {
            if (consider(i))
            {
                objective_ents_.push_back(i);
            }
        }
    }
    else
    {
        for (int i : objective_ents_)
        {
            consider(i);
        }
    }
}

void ObjectivesLayer::DrawHostages(const RadarFrame& frame)
{
    if (g_RadarCfg.objectives->value == 0.0f)
    {
        return;
    }

    if (!frame.ctx.local)
    {
        return;
    }

    int dot = std::max(2, (int)(frame.box.elem * kHostageDotScale));

    if (policy_.hostage_intent() == HostageIntent::Marker)
    {
        DrawServerHostageMarkers(frame, dot);
        return;
    }

    auto draw_dot = [&](float wx, float wy, int alpha) {
        ncl_math::Vector2 s = RadarView_ClampToRadar(frame.box, frame.round, dot, frame.t.WorldToScreen({wx, wy}));
        RadarDump::AddMarker("hostage", -1, wx, wy, s.x, s.y, 0, 220, 220, alpha, "");
        RadarDraw_DrawDisc(s, (float)dot, {0, 220, 220, alpha});
    };

    bool spectating = frame.ctx.spectating;
    int my_team = frame.ctx.local_team;
    // Terrorists are never told where the hostages are, and neither is a CT whose server
    // withholds the broadcast: both then see only what they can actually look at, rather
    // than everything the PVS happens to carry.
    bool los_gated = !spectating && (my_team == TEAM_TERRORIST || !policy_.hostages_revealed());

    // Smooth the marker by easing the spot toward the live entity each frame, so it
    // glides instead of stepping at the snapshot rate (the CT broadcast is coarser
    // still).
    float dt = std::clamp(frame.dt, 0.0f, 0.1f);
    auto& spots = obj_memory_.hostages();

    // Terrorists reveal a hostage only by line of sight (throttled) and fade it over a
    // short memory window after sight is lost.
    if (los_gated)
    {
        if (obj_test_due_)
        {
            obj_memory_.ClearHostageVisibility();

            for (int i = 0; i < frame_ents_.hostage_count; i++)
            {
                const FrameEntities::HostageEnt& h = frame_ents_.hostages[i];

                if (!visibility_.PointVisible(h.origin, frame.time))
                {
                    continue;
                }

                int slot = obj_memory_.FindOrCreate(h.index, {h.origin.x, h.origin.y}, frame.time);

                if (slot >= 0)
                {
                    spots[slot].visible = true;
                    spots[slot].last_seen = frame.time;
                }
            }
        }

        for (int i = 0; i < frame_ents_.hostage_count; i++)
        {
            const FrameEntities::HostageEnt& h = frame_ents_.hostages[i];

            for (ObjectiveMemory::HostageSpot& s : spots)
            {
                if (s.valid && s.visible && s.ent == h.index)
                {
                    obj_memory_.EaseTo(s, {h.origin.x, h.origin.y}, dt);
                    break;
                }
            }
        }

        RadarDraw_BeginPrims();

        for (ObjectiveMemory::HostageSpot& s : spots)
        {
            if (!s.valid)
            {
                continue;
            }

            float age = frame.time - s.last_seen;

            if (age < 0.0f || age > ObjectiveMemory::kMemory)
            {
                s.valid = false;
                continue;
            }

            float fade = ncl_math::HoldLinear(age, ObjectiveMemory::kHold, ObjectiveMemory::kMemory);
            draw_dot(s.pos.x, s.pos.y, (int)(255.0f * fade));
        }

        RadarDraw_EndPrims();
        return;
    }

    // Counter-Terrorists and spectators show every hostage the PVS provides, eased.
    obj_memory_.ClearHostageVisibility();

    for (int i = 0; i < frame_ents_.hostage_count; i++)
    {
        const FrameEntities::HostageEnt& h = frame_ents_.hostages[i];
        int slot = obj_memory_.FindOrCreate(h.index, {h.origin.x, h.origin.y}, frame.time);

        if (slot >= 0)
        {
            obj_memory_.EaseTo(spots[slot], {h.origin.x, h.origin.y}, dt);
            spots[slot].visible = true;
        }
    }

    RadarDraw_BeginPrims();

    for (ObjectiveMemory::HostageSpot& s : spots)
    {
        if (!s.valid)
        {
            continue;
        }

        if (!s.visible)
        {
            s.valid = false; // left the PVS; the broadcast covers it for CTs
            continue;
        }

        draw_dot(s.pos.x, s.pos.y, 255);
    }

    // CTs additionally show the broadcast hostages outside the PVS (no live entity near
    // them), so out-of-sight hostages still appear as on the stock radar. The broadcast
    // lags, so an in-PVS hostage is drawn from its live (smooth) entity above and its
    // broadcast copy is skipped here to avoid a doubled, jittery marker.
    if (!spectating && my_team == TEAM_CT)
    {
        RadarBroadcast::Hostage hs[24];
        int hn = RadarBroadcast::CollectHostages(hs, 24);

        for (int i = 0; i < hn; i++)
        {
            bool covered = false;

            for (int k = 0; k < frame_ents_.hostage_count; k++)
            {
                ncl_math::Vector2 d = ncl_math::Vector2{frame_ents_.hostages[k].origin.x, frame_ents_.hostages[k].origin.y} -
                    hs[i].pos;

                if (d.LengthSqr() < kHostageMatchDist * kHostageMatchDist)
                {
                    covered = true;
                    break;
                }
            }

            if (!covered)
            {
                draw_dot(hs[i].pos.x, hs[i].pos.y, 255);
            }
        }
    }

    RadarDraw_EndPrims();
}

void ObjectivesLayer::DrawBomb(const RadarFrame& frame)
{
    if (g_RadarCfg.objectives->value == 0.0f)
    {
        return;
    }

    if (!frame.ctx.local)
    {
        return;
    }

    int my_team = frame.ctx.local_team;

    int sz = std::max(3, (int)(frame.box.elem * kBombDotScale));

    if (policy_.bomb_intent() == BombIntent::Marker)
    {
        DrawServerMarker(frame, sz);
        return;
    }

    marker_valid_ = false;

    // Render time the engine interpolates entities to, so the bomb dot follows the
    // same smooth path as the world model rather than the discrete snapshot origin.
    if (!ex_interp_)
    {
        ex_interp_ = gEngfuncs.pfnGetCvarPointer("ex_interp");
    }

    float ex = ex_interp_ ? ex_interp_->value : 0.0f;

    if (ex <= 0.0f || ex > 0.1f)
    {
        ex = 0.1f;
    }

    float interp_target = gEngfuncs.GetClientTime() - ex;

    auto draw_bomb = [&](ncl_math::Vector2 world, bool planted, float fade) {
        ncl_math::Vector2 s = RadarView_ClampToRadar(frame.box, frame.round, sz, frame.t.WorldToScreen(world));
        float sx = s.x, sy = s.y;

        float f = std::clamp(fade, 0.0f, 1.0f);

        RadarDraw_BeginPrims();

        int a = 255;

        if (planted)
        {
            // Radar-ping rings expanding out of the planted bomb. Two staggered
            // rings keep the pulse continuous; each fades as it grows.
            float since = std::max(0.0f, frame.time - bomb_pulse_start_); // from plant time

            for (int k = 0; k < 2; k++)
            {
                // Each ring only starts once its launch time is reached, so the
                // first wave leaves the centre at plant time and the second begins
                // half a period later instead of appearing pre-expanded.
                float tk = since - k * 0.5f * kBombPulsePeriod;

                if (tk < 0.0f)
                {
                    continue;
                }

                float phase = std::fmod(tk / kBombPulsePeriod, 1.0f);
                float rr = sz * (1.2f + phase * 5.5f);
                int ra = (int)((1.0f - phase) * 150.0f * f);

                if (ra > 0)
                {
                    RadarDraw_DrawRing({sx, sy}, rr, rr - 2.5f, RadarPalette::kGold.WithAlpha(ra));
                }
            }

            float pulse = 0.5f + 0.5f * std::sin(frame.time * 3.0f);
            a = (int)(110 + 145 * pulse);
        }

        a = (int)(a * f);

        RadarDump::AddMarker(
            "bomb", 0, world.x, world.y, sx, sy, RadarPalette::kGold.r, RadarPalette::kGold.g, RadarPalette::kGold.b, a,
            planted ? "planted" : ""
        );

        // Yellow keeps the bomb's visual language consistent with the carrier ring.
        RadarDraw_DrawDisc({sx, sy}, (float)sz + 1.5f, {0, 0, 0, a});
        RadarDraw_DrawDisc({sx, sy}, (float)sz, RadarPalette::kGold.WithAlpha(a));
        RadarDraw_DrawDisc({sx, sy}, sz * 0.5f, {90, 70, 0, a});

        RadarDraw_EndPrims();
    };

    // Terrorists are told where the dropped/planted bomb lies via BombDrop and
    // show it from anywhere (out of PVS included).
    if (my_team == TEAM_TERRORIST)
    {
        const RadarBroadcast::Bomb& bomb = RadarBroadcast::GetBomb();

        if (!bomb.active)
        {
            bomb_pulse_start_ = -1.0f;
            bomb_appear_time_ = -1.0f;
            return;
        }

        if (bomb_appear_time_ < 0.0f)
        {
            bomb_appear_time_ = frame.time;
        }

        // Hold a freshly dropped bomb off the radar for ex_interp so the entity's
        // position history can fill; it then appears already in flight at its
        // interpolated position, with no catch-up jerk. Planted shows immediately.
        if (!bomb.planted && frame.time - bomb_appear_time_ < ex)
        {
            return;
        }

        ncl_math::Vector2 bomb_xy{bomb.pos.x, bomb.pos.y};
        bool planted = bomb.planted;

        // Follow the interpolated bomb entity (like its world model) while in PVS;
        // otherwise use the broadcast position.
        if (frame_ents_.bomb.found)
        {
            bomb_xy = frame_ents_.bomb.pos;
            InterpEntityXY(frame_ents_.bomb.ent, interp_target, &bomb_xy);
            planted = frame_ents_.bomb.planted;
        }

        if (planted)
        {
            if (bomb_pulse_start_ < 0.0f)
            {
                bomb_pulse_start_ = frame.time;
            }
        }
        else
        {
            bomb_pulse_start_ = -1.0f;
        }

        draw_bomb(bomb_xy, planted, 1.0f);
        return;
    }

    if (my_team != TEAM_CT)
    {
        return;
    }

    // Counter-Terrorists get no bomb message; spot the bomb entity while in direct
    // line of sight (dropped uses the w_backpack world model, planted uses w_c4),
    // then fade its marker out over a short memory window after sight is lost.
    ObjectiveMemory::BombSpot& spot = obj_memory_.bomb();

    if (frame_ents_.bomb.found && visibility_.PointVisible(ncl_math::Vector3::Ref(frame_ents_.bomb.ent->origin), frame.time))
    {
        spot.valid = true;
        spot.pos = frame_ents_.bomb.pos;
        InterpEntityXY(frame_ents_.bomb.ent, interp_target, &spot.pos);
        spot.planted = frame_ents_.bomb.planted;
        spot.last_seen = frame.time;
    }

    if (!spot.valid)
    {
        bomb_pulse_start_ = -1.0f;
        return;
    }

    float age = frame.time - spot.last_seen;

    if (age < 0.0f || age > ObjectiveMemory::kMemory)
    {
        spot.valid = false;
        bomb_pulse_start_ = -1.0f;
        return;
    }

    if (spot.planted)
    {
        if (bomb_pulse_start_ < 0.0f)
        {
            bomb_pulse_start_ = frame.time;
        }
    }
    else
    {
        bomb_pulse_start_ = -1.0f;
    }

    float fade = ncl_math::HoldLinear(age, ObjectiveMemory::kHold, ObjectiveMemory::kMemory);
    draw_bomb(spot.pos, spot.planted, fade);
}

// A bomb message the server re-purposed to mark something else, typically the last
// survivor on a zombie server. It is drawn for whoever the server sent it to (the stock
// team split does not apply to a mode that has no bomb) and never gated by line of
// sight, since the reveal is the server's own doing. No bomb entity backs it, so the
// position is eased rather than interpolated, and it expires on its own because the
// BombPickup that would clear a real bomb may never arrive.
void ObjectivesLayer::DrawServerMarker(const RadarFrame& frame, int size)
{
    const RadarBroadcast::Bomb& bomb = RadarBroadcast::GetBomb();

    if (!bomb.active || !policy_.marker_visible())
    {
        marker_valid_ = false;
        return;
    }

    ncl_math::Vector2 target{bomb.pos.x, bomb.pos.y};

    if (marker_valid_)
    {
        marker_pos_ = ObjectiveMemory::Ease(marker_pos_, target, std::clamp(frame.dt, 0.0f, 0.1f));
    }
    else
    {
        marker_pos_ = target;
        marker_valid_ = true;
    }

    ncl_math::Vector2 s = RadarView_ClampToRadar(frame.box, frame.round, size, frame.t.WorldToScreen(marker_pos_));
    ncl_math::Color col = RadarPalette::kServerMarker;

    RadarDump::AddMarker("marker", 0, marker_pos_.x, marker_pos_.y, s.x, s.y, col.r, col.g, col.b, 255, "bomb");

    RadarDraw_BeginPrims();
    DrawMarkerShape(s, size);
    RadarDraw_EndPrims();
}

// The hostage message put to the same use: one marker per index the server keeps
// refreshing, shown to whoever it was sent to and expiring on its own, since the
// HostageK that would clear a real hostage may never come.
void ObjectivesLayer::DrawServerHostageMarkers(const RadarFrame& frame, int size)
{
    RadarBroadcast::Hostage broadcast[FrameEntities::kMaxHostages];
    int count = RadarBroadcast::CollectHostages(broadcast, FrameEntities::kMaxHostages);
    float dt = std::clamp(frame.dt, 0.0f, 0.1f);
    auto& spots = obj_memory_.hostages();

    obj_memory_.ClearHostageVisibility();

    for (int i = 0; i < count; i++)
    {
        if (!policy_.HostageMarkerVisible(broadcast[i].index))
        {
            continue;
        }

        int slot = obj_memory_.FindOrCreate(broadcast[i].index, broadcast[i].pos, frame.time);

        if (slot < 0)
        {
            continue;
        }

        // The server refreshes a marker at its own rate and no entity backs it, so the
        // position is eased rather than interpolated.
        obj_memory_.EaseTo(spots[slot], broadcast[i].pos, dt);
        spots[slot].visible = true;
    }

    ncl_math::Color col = RadarPalette::kServerMarker;

    RadarDraw_BeginPrims();

    for (ObjectiveMemory::HostageSpot& s : spots)
    {
        if (!s.valid)
        {
            continue;
        }

        if (!s.visible)
        {
            s.valid = false; // expired or cleared by the server
            continue;
        }

        ncl_math::Vector2 p = RadarView_ClampToRadar(frame.box, frame.round, size, frame.t.WorldToScreen(s.pos));
        RadarDump::AddMarker("marker", s.ent, s.pos.x, s.pos.y, p.x, p.y, col.r, col.g, col.b, 255, "hostage");
        DrawMarkerShape(p, size);
    }

    RadarDraw_EndPrims();
}

void ObjectivesLayer::DrawMarkerShape(ncl_math::Vector2 screen, int size)
{
    ncl_math::Color col = RadarPalette::kServerMarker;
    RadarDraw_DrawRing(screen, (float)size + 1.5f, (float)size - 0.5f, col);
    RadarDraw_DrawCross(screen, size * 0.75f, 1.5f, col);
}

void ObjectivesLayer::DrawSites(const RadarFrame& frame)
{
    if (g_RadarCfg.objectives->value == 0.0f)
    {
        return;
    }

    objectives_.EnsureLoaded();

    int site_px = SiteHeightPx(frame);
    bool batched = RadarText_Available(site_px);

    float flash_fade = batched ? RadarLabel_FlashFade(frame.flash_alpha) : (frame.flash_alpha >= kLabelFlashHide ? 0.0f : 1.0f);

    if (flash_fade <= 0.0f)
    {
        return;
    }

    if (batched)
    {
        RadarDraw_BeginPrims();
    }

    // Fall back to the engine font rather than dropping the letters when the glyph service is
    // unavailable; the label is a single character and carries no performance argument.
    int fallback_h = 0;

    if (!batched)
    {
        int probe_w = 0;
        gEngfuncs.pfnDrawConsoleStringLen("x", &probe_w, &fallback_h);
    }

    for (int i = 0; i < objectives_.site_count(); i++)
    {
        const BombSite& site = objectives_.site(i);

        ncl_math::Vector2 s = RadarView_ClampToRadar(frame.box, frame.round, 6, frame.t.WorldToScreen({site.pos.x, site.pos.y}));

        RadarDump::AddMarker(
            "site", i, site.pos.x, site.pos.y, s.x, s.y, RadarPalette::kGold.r, RadarPalette::kGold.g, RadarPalette::kGold.b, 255,
            site.label
        );

        if (batched)
        {
            // Sites take their own slot range: the layout cache is keyed on the slot, and the
            // player pass owns 0..kMaxPlayers.
            ncl_math::Color gold = RadarPalette::kGold.WithAlpha((int)(255 * flash_fade));
            RadarText_DrawCentered(kSiteSlotBase + i, site.label, {s.x, s.y - site_px * 0.5f}, site_px, gold);
        }
        else
        {
            int tw = 0, th = 0;
            gEngfuncs.pfnDrawConsoleStringLen(site.label, &tw, &th);
            RadarLabel_Draw(site.label, (int)s.x - tw / 2, (int)s.y - fallback_h / 2, RadarPalette::kGold, frame.flash_alpha);
        }
    }

    if (batched)
    {
        RadarDraw_EndPrims();
    }
}

int ObjectivesLayer::SiteHeightPx(const RadarFrame& frame)
{
    // Match the original bomb-site letters, which are the engine's standard scheme font at its
    // resolution size. Fall back to the box-relative ladder only until that font resolves (the
    // engine-font fallback path draws in the meantime).
    int standard = RadarText_StandardPx();

    if (standard > 0)
    {
        return standard;
    }

    return std::clamp((int)std::lround(frame.box.elem * kSiteHeightScale), kSiteMinPx, kSiteMaxPx);
}
