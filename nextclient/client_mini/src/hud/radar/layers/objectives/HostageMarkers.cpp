#include "HostageMarkers.h"

#include <algorithm>

#include "main.h"

#include <ncl_math/scalar.h>

#include "hud/radar/RadarBroadcast.h"
#include "hud/radar/RadarDraw.h"
#include "hud/radar/RadarLimits.h"
#include "hud/radar/RadarPolicy.h"
#include "hud/radar/ServerMarker.h"
#include "hud/radar/Visibility.h"

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

int HostageMarkers::FindOrCreate(int key, ncl_math::Vector2 pos, float time)
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

void HostageMarkers::DrawDot(const RadarFrame& frame, ncl_math::Vector2 world, int dot, int alpha)
{
    RadarDraw_DrawDisc(frame.shape.Clamp(dot, frame.t.WorldToScreen(world)), (float)dot, {0, 220, 220, alpha});
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

            int slot = FindOrCreate(h.index, {h.origin.x, h.origin.y}, frame.time);

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
            spot->pos = {hostage_ent.origin.x, hostage_ent.origin.y};
        }
    }

    RadarDraw_BeginPrims();

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

    RadarDraw_EndPrims();
}

void HostageMarkers::DrawRevealed(const RadarFrame& frame, const ObjectiveEntities& entities, int dot)
{
    MarkerIdentity::Entity carried[kRadarMaxHostages];
    int carried_count = 0;

    for (const ObjectiveEntities::HostageEnt& hostage : entities.get_hostages())
    {
        carried[carried_count++] = {hostage.index, {hostage.origin.x, hostage.origin.y}};
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
            broadcast[broadcast_count++] = {hostages[i].index, hostages[i].pos, hostages[i].seq};
        }
    }

    feed_.Update({carried, carried + carried_count}, {broadcast, broadcast + broadcast_count}, frame.time);

    RadarDraw_BeginPrims();

    for (const MarkerFeed::Marker& marker : feed_.get_markers())
    {
        DrawDot(frame, marker.pos, dot, 255);
    }

    RadarDraw_EndPrims();
}

// The hostage message put to the same use: it expires on its own, since the clearing HostageK may
// never come.
void HostageMarkers::DrawServerMarkers(const RadarFrame& frame, int size)
{
    RadarBroadcast::Hostage broadcast[kRadarMaxHostages];
    int count = broadcast_.CollectHostages(broadcast);

    ClearVisibility();

    for (int i = 0; i < count; i++)
    {
        if (!policy_.IsHostageMarkerVisible(broadcast[i].index))
        {
            continue;
        }

        int slot = FindOrCreate(broadcast[i].index, broadcast[i].pos, frame.time);

        if (slot < 0)
        {
            continue;
        }

        spots_[slot].pos = ServerMarker_Ease(spots_[slot].pos, broadcast[i].pos, frame.dt);
        spots_[slot].visible = true;
    }

    RadarDraw_BeginPrims();

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

        ncl_math::Vector2 p = frame.shape.Clamp(size, frame.t.WorldToScreen(spot.pos));
        ServerMarker_DrawShape(p, size);
    }

    RadarDraw_EndPrims();
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
