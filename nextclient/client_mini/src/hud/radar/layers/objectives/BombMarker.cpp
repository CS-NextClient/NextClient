#include "BombMarker.h"

#include <algorithm>
#include <cmath>

#include "main.h"

#include <ncl_math/scalar.h>
#include <goldsrc/client_api.h>
#include <goldsrc/cvar.h>
#include <goldsrc/entity.h>

#include "hud/radar/RadarBroadcast.h"
#include "hud/radar/RadarDraw.h"
#include "hud/radar/RadarPalette.h"
#include "hud/radar/RadarPolicy.h"
#include "hud/radar/ServerMarker.h"
#include "hud/radar/Visibility.h"

namespace
{
    constexpr float kBombPulsePeriod = 4.4f; // seconds

    // How long the marker outlives the sight of it. Distinct from RadarVisibility's enemy spot memory.
    constexpr float kSightMemory = 1.5f; // seconds
    constexpr float kSightHold = 0.4f; // seconds, inside kSightMemory

    constexpr float kEaseRate = 16.0f; // 1/s
    constexpr float kSnapDist = 200.0f; // world units

    constexpr int kHistoryMask = goldsrc::HISTORY_MAX - 1;

    // Snaps instead of easing past kSnapDist, so a re-purposed message does not sweep across the map.
    ncl_math::Vector2 EaseMarker(ncl_math::Vector2 pos, ncl_math::Vector2 target, float dt)
    {
        ncl_math::Vector2 dir = target - pos;

        if (dir.LengthSqr() > kSnapDist * kSnapDist)
        {
            return target;
        }

        return pos + dir * ncl_math::SmoothingFactor(kEaseRate, dt);
    }

    // Mirrors the engine's CL_InterpolateModel; ent->origin alone steps at the server update rate.
    bool InterpEntityXY(goldsrc::cl_entity_t* ent, float target, ncl_math::Vector2& out)
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

        const goldsrc::position_history_t& ph0 = ent->ph[idx0];
        const goldsrc::position_history_t& ph1 = ent->ph[idx1];

        if (ph1.animtime == 0.0f || ph0.animtime <= ph1.animtime)
        {
            return false;
        }

        float scale = std::clamp((target - ph1.animtime) / (ph0.animtime - ph1.animtime), 0.0f, 1.0f);
        ncl_math::Vector2 p0{ph0.origin[0], ph0.origin[1]};
        ncl_math::Vector2 p1{ph1.origin[0], ph1.origin[1]};
        out = ncl_math::Lerp(p1, p0, scale);
        return true;
    }
} // namespace

BombMarker::BombMarker(RadarVisibility& visibility, const RadarPolicy& policy, const RadarBroadcast& broadcast) :
    visibility_(visibility),
    policy_(policy),
    broadcast_(broadcast)
{
}

void BombMarker::Reset()
{
    spot_ = Spot{};
    pulse_start_ = -1.0f;
    marker_valid_ = false;
    appear_time_ = -1.0f;
    visible_ = false;
}

float BombMarker::ComputeInterpLag()
{
    if (!ex_interp_)
    {
        ex_interp_ = gEngfuncs.pfnGetCvarPointer("ex_interp");
    }

    float ex = ex_interp_ ? ex_interp_->value : 0.0f;

    return (ex <= 0.0f || ex > 0.1f) ? 0.1f : ex;
}

void BombMarker::UpdatePulse(bool planted, float time)
{
    if (!planted)
    {
        pulse_start_ = -1.0f;
        return;
    }

    if (pulse_start_ < 0.0f)
    {
        pulse_start_ = time;
    }
}

void BombMarker::DrawMarker(const RadarFrame& frame, ncl_math::Vector2 world, int size, bool planted, float fade)
{
    ncl_math::Vector2 s = frame.shape.Clamp(size, frame.t.WorldToScreen(world));
    float f = std::clamp(fade, 0.0f, 1.0f);

    RadarDraw_BeginPrims();

    int a = 255;

    if (planted)
    {
        float since = std::max(0.0f, frame.time - pulse_start_);

        for (int k = 0; k < 2; k++)
        {
            // Skip a ring until its launch time, or the second wave appears pre-expanded.
            float tk = since - k * 0.5f * kBombPulsePeriod;

            if (tk < 0.0f)
            {
                continue;
            }

            float phase = std::fmod(tk / kBombPulsePeriod, 1.0f);
            float rr = size * (1.2f + phase * 5.5f);
            int ra = (int)((1.0f - phase) * 150.0f * f);

            if (ra > 0)
            {
                RadarDraw_DrawRing(s, rr, rr - 2.5f, RadarPalette::kGold.WithAlpha(ra));
            }
        }

        float pulse = 0.5f + 0.5f * std::sin(frame.time * 3.0f);
        a = (int)(110 + 145 * pulse);
    }

    a = (int)(a * f);

    RadarDraw_DrawDisc(s, (float)size + 1.5f, {0, 0, 0, a});
    RadarDraw_DrawDisc(s, (float)size, RadarPalette::kGold.WithAlpha(a));
    RadarDraw_DrawDisc(s, size * 0.5f, {90, 70, 0, a});

    RadarDraw_EndPrims();
}

void BombMarker::DrawFromBroadcast(const RadarFrame& frame, const ObjectiveEntities& entities, int size, float interp_lag)
{
    const RadarBroadcast::Bomb& bomb = broadcast_.get_bomb();

    if (!bomb.active)
    {
        pulse_start_ = -1.0f;
        appear_time_ = -1.0f;
        return;
    }

    if (appear_time_ < 0.0f)
    {
        appear_time_ = frame.time;
    }

    // Hold a dropped bomb for ex_interp so its position history fills, else it jerks to catch up.
    if (!bomb.planted && frame.time - appear_time_ < interp_lag)
    {
        return;
    }

    ncl_math::Vector2 bomb_xy{bomb.pos.x, bomb.pos.y};
    bool planted = bomb.planted;
    const ObjectiveEntities::BombEnt& live = entities.get_bomb();

    if (live.found)
    {
        bomb_xy = live.pos;
        InterpEntityXY(live.ent, gEngfuncs.GetClientTime() - interp_lag, bomb_xy);
        planted = live.planted;
    }

    UpdatePulse(planted, frame.time);
    DrawMarker(frame, bomb_xy, size, planted, 1.0f);
}

void BombMarker::DrawFromSight(const RadarFrame& frame, const ObjectiveEntities& entities, int size, float interp_lag)
{
    const ObjectiveEntities::BombEnt& live = entities.get_bomb();

    if (!live.found)
    {
        visible_ = false;
    }
    else
    {
        // RadarVisibility defers its physent rebuild to the frame's first test, so untested frames are free.
        if (entities.is_test_due())
        {
            visible_ = visibility_.CheckPointVisible(ncl_math::Vector3::Ref(live.ent->origin), frame.time);
        }

        if (visible_)
        {
            spot_.valid = true;
            spot_.pos = live.pos;
            InterpEntityXY(live.ent, gEngfuncs.GetClientTime() - interp_lag, spot_.pos);
            spot_.planted = live.planted;
            spot_.last_seen = frame.time;
        }
    }

    if (!spot_.valid)
    {
        pulse_start_ = -1.0f;
        return;
    }

    float age = frame.time - spot_.last_seen;

    if (age < 0.0f || age > kSightMemory)
    {
        spot_.valid = false;
        pulse_start_ = -1.0f;
        return;
    }

    UpdatePulse(spot_.planted, frame.time);
    DrawMarker(frame, spot_.pos, size, spot_.planted, ncl_math::HoldLinear(age, kSightHold, kSightMemory));
}

// A re-purposed bomb message: never gated by line of sight, since the reveal is the server's own
// doing, and it expires on its own because the clearing BombPickup may never arrive.
void BombMarker::DrawServerMarker(const RadarFrame& frame, int size)
{
    const RadarBroadcast::Bomb& bomb = broadcast_.get_bomb();

    if (!bomb.active || !policy_.is_marker_visible())
    {
        marker_valid_ = false;
        return;
    }

    ncl_math::Vector2 target{bomb.pos.x, bomb.pos.y};

    if (marker_valid_)
    {
        marker_pos_ = EaseMarker(marker_pos_, target, std::clamp(frame.dt, 0.0f, 0.1f));
    }
    else
    {
        marker_pos_ = target;
        marker_valid_ = true;
    }

    ncl_math::Vector2 s = frame.shape.Clamp(size, frame.t.WorldToScreen(marker_pos_));
    RadarDraw_BeginPrims();
    ServerMarker_DrawShape(s, size);
    RadarDraw_EndPrims();
}

void BombMarker::Draw(const RadarFrame& frame, const ObjectiveEntities& entities, int size)
{
    if (policy_.get_bomb_intent() == BombIntent::Marker)
    {
        DrawServerMarker(frame, size);
        return;
    }

    // Kept above the audience dispatch: inside a branch, a Terrorist frame leaves this stale and a
    // later mode flip snaps the marker across the map.
    marker_valid_ = false;

    // Spectators read the broadcast too: BombDrop goes to everyone, and the line-of-sight path
    // cannot serve them (RadarVisibility keeps no viewers while spectating).
    if (frame.ctx.local_team == RadarTeam::Terrorist || frame.ctx.spectating)
    {
        DrawFromBroadcast(frame, entities, size, ComputeInterpLag());
        return;
    }

    // Counter-Terrorists get no bomb message.
    if (frame.ctx.local_team == RadarTeam::CounterTerrorist)
    {
        DrawFromSight(frame, entities, size, ComputeInterpLag());
    }
}
