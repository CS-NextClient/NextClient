#include "HostageMarkers.h"

#include <algorithm>
#include <cmath>

#include "main.h"

#include <ncl_math/scalar.h>

#include "hud/radar/markers/RadarBroadcast.h"
#include "hud/radar/render/RadarDraw.h"
#include "hud/radar/RadarLimits.h"
#include "hud/radar/policy/RadarPolicy.h"
#include "hud/radar/markers/RadarLayerMarker.h"
#include "hud/radar/render/RadarPalette.h"
#include "hud/radar/markers/ServerMarker.h"
#include "hud/radar/policy/Visibility.h"

namespace
{
    // CHostage reports a moved hostage once a second, walks at up to 400 u/s, and its message
    // index is a spawn counter with no relation to the edict the snapshot carries.
    constexpr MarkerFeed::Params kFeedParams{
        .identity = {.match_tolerance = 48.0f, .break_strikes = 3},
        .min_interval = 0.2f,
        .max_interval = 1.5f,
        .snap_dist = 600.0f,
    };
} // namespace

HostageMarkers::HostageMarkers(RadarVisibility& visibility, const RadarPolicy& policy, const RadarBroadcast& broadcast) :
    visibility_(visibility),
    policy_(policy),
    broadcast_(broadcast),
    feed_(kFeedParams)
{
}

void HostageMarkers::Reset()
{
    spots_.fill(Spot{});
    feed_.ResetMotion();
}

void HostageMarkers::OnLevelChange()
{
    feed_.Reset();
}

void HostageMarkers::SetPass(HostageSource pass)
{
    if (pass == pass_)
    {
        return;
    }

    pass_ = pass;
    spots_.fill(Spot{});
    feed_.ResetMotion();
}

void HostageMarkers::ClearVisibility()
{
    for (Spot& spot : spots_)
    {
        spot.visible = false;
    }
}

int HostageMarkers::FindOrCreate(int key, ncl_math::Vector3 pos, float time)
{
    for (int i = 0; i < kRadarMaxHostages; i++)
    {
        if (spots_[i].valid && spots_[i].key == key)
        {
            return i;
        }
    }

    for (int i = 0; i < kRadarMaxHostages; i++)
    {
        if (!spots_[i].valid)
        {
            spots_[i] = {true, key, pos, time, false};
            return i;
        }
    }

    return -1;
}

void HostageMarkers::DrawDot(const RadarFrame& frame, ncl_math::Vector3 world, int dot, int alpha)
{
    RadarLayerStyle style = RadarLayerMarker_ComputeStyle(frame.layer_view, world);
    RadarLayerPlacement placement = RadarLayerMarker_Place(frame.shape, frame.t.WorldToScreen({world.x, world.y}), dot, style);
    ncl_math::Color color{0, 220, 220, (int)(alpha * style.opacity)};

    frame.draw.DrawDisc(placement.marker, (float)dot, color);
    frame.draw.DrawLayerIndicator(placement.indicator, style.vertical, dot, color);
}

void HostageMarkers::DrawBySight(const RadarFrame& frame, const ObjectiveEntities& entities, int dot)
{
    if (entities.is_test_due())
    {
        ClearVisibility();

        for (const ObjectiveEntities::HostageEnt& h : entities.get_hostages())
        {
            if (!visibility_.CheckPointVisible(h.origin, frame.time))
            {
                continue;
            }

            int slot = FindOrCreate(h.index, h.origin, frame.time);

            if (slot >= 0)
            {
                spots_[slot].visible = true;
                spots_[slot].last_seen = frame.time;
            }
        }
    }

    for (const ObjectiveEntities::HostageEnt& hostage_ent : entities.get_hostages())
    {
        auto spot = std::ranges::find_if(spots_, [&hostage_ent](const Spot& spot) {
            return spot.valid && spot.visible && spot.key == hostage_ent.index;
        });

        if (spot != spots_.end())
        {
            spot->pos = hostage_ent.origin;
        }
    }

    frame.draw.BeginPrims();

    // Age expiry, not the PVS pass's drop-on-leave: nothing to fall back on once sight is lost.
    for (Spot& spot : spots_)
    {
        if (!spot.valid)
        {
            continue;
        }

        float age = frame.time - spot.last_seen;

        if (age < 0.0f || age > kObjectiveSightMemory)
        {
            spot.valid = false;
            continue;
        }

        float fade = ncl_math::HoldLinear(age, kObjectiveSightHold, kObjectiveSightMemory);
        DrawDot(frame, spot.pos, dot, (int)(255.0f * fade));
    }

    frame.draw.EndPrims();
}

void HostageMarkers::DrawRevealed(const RadarFrame& frame, const ObjectiveEntities& entities, int dot)
{
    MarkerIdentity::Entity carried[kRadarMaxHostages];
    int carried_count = 0;

    for (const ObjectiveEntities::HostageEnt& hostage : entities.get_hostages())
    {
        carried[carried_count++] = {hostage.index, hostage.origin, hostage.discontinuity};
    }

    MarkerIdentity::Observation broadcast[kRadarMaxHostages];
    int broadcast_count = 0;

    // The message reaches living Counter-Terrorists only; anyone else holds a stale table.
    if (!frame.ctx.spectating && frame.ctx.local_team == RadarTeam::CounterTerrorist)
    {
        RadarBroadcast::Hostage hostages[kRadarMaxHostages];
        int count = broadcast_.CollectHostages(hostages);

        for (int i = 0; i < count; i++)
        {
            broadcast[broadcast_count++] = {
                hostages[i].index, hostages[i].pos, hostages[i].seq, hostages[i].previous_pos, hostages[i].interval
            };
        }
    }

    feed_.Update({carried, carried + carried_count}, {broadcast, broadcast + broadcast_count}, frame.time);

    frame.draw.BeginPrims();

    for (const MarkerFeed::Marker& marker : feed_.get_markers())
    {
        DrawDot(frame, marker.pos, dot, 255);
    }

    frame.draw.EndPrims();
}

// The hostage message put to the same use: it expires on its own, since the clearing HostageK may
// never come.
void HostageMarkers::DrawServerMarkers(const RadarFrame& frame, int size)
{
    RadarBroadcast::Hostage broadcast[kRadarMaxHostages];
    int count = broadcast_.CollectHostageMarkers(broadcast, policy_);

    ClearVisibility();

    for (Spot& spot : spots_)
    {
        if (spot.valid && !policy_.IsHostageMarkerVisible(spot.key))
        {
            spot.valid = false;
        }
    }

    for (int i = 0; i < count; i++)
    {
        int slot = FindOrCreate(broadcast[i].index, broadcast[i].pos, frame.time);

        if (slot < 0)
        {
            continue;
        }

        spots_[slot].pos = ServerMarker_Ease(spots_[slot].pos, broadcast[i].pos, frame.dt);
        spots_[slot].visible = true;
    }

    frame.draw.BeginPrims();

    for (Spot& spot : spots_)
    {
        if (!spot.valid)
        {
            continue;
        }

        if (!spot.visible)
        {
            spot.valid = false; // expired or cleared by the server
            continue;
        }

        RadarLayerStyle style = RadarLayerMarker_ComputeStyle(frame.layer_view, spot.pos);
        int extent = frame.layer_view.enabled ? (int)std::ceil(size + 1.5f) : size;
        RadarLayerPlacement placement =
            RadarLayerMarker_Place(frame.shape, frame.t.WorldToScreen({spot.pos.x, spot.pos.y}), extent, style, size);
        ncl_math::Color color = RadarPalette::kServerMarker.WithAlpha((int)(255 * style.opacity));

        ServerMarker_DrawShape(frame.draw, placement.marker, size, color);
        frame.draw.DrawLayerIndicator(placement.indicator, style.vertical, size, color);
    }

    frame.draw.EndPrims();
}

void HostageMarkers::Draw(const RadarFrame& frame, const ObjectiveEntities& entities, int dot)
{
    HostageSource source = ObjectiveSource_ChooseHostages(
        policy_.get_hostage_intent(), {frame.ctx.local_team, frame.ctx.spectating}, policy_.are_hostages_revealed());

    SetPass(source);

    switch (source)
    {
        case HostageSource::ServerMarker:
            DrawServerMarkers(frame, dot);
            break;
        case HostageSource::Sight:
            DrawBySight(frame, entities, dot);
            break;
        case HostageSource::Revealed:
            DrawRevealed(frame, entities, dot);
            break;
    }
}
