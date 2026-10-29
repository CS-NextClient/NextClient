#include "Visibility.h"

#include <cmath>
#include <cstring>

#include "main.h"

#include "hud/radar/integration/Performance.h"

#include <ncl_math/vec3.h>
#include <goldsrc/client_api.h>
#include <goldsrc/cstrike/hud.h>
#include <goldsrc/entity.h>
#include <goldsrc/event_api.h>
#include <goldsrc/pmove.h>
#include <goldsrc/view.h>

#include "hud/radar/policy/SmokeRegistry.h"
#include "hud/radar/render/RadarFade.h"
#include "hud/radar/policy/RadarPolicy.h"
#include "hud/radar/integration/RadarUtil.h"
#include "hud/radar/RadarFrame.h"
#include "view.h"

namespace
{
    constexpr float kVisInterval = 0.05f;
    constexpr float kEyeZ = goldsrc::PM_VEC_VIEW;
    constexpr float kDuckEyeZ = goldsrc::PM_VEC_DUCK_VIEW;
    // The second trace point on a target, just above eye level.
    // (the standing hull reaches +36, and testing its top would spot a player whose head is still behind cover)
    constexpr float kHeadEyeOffset = 5.0f;
    constexpr float kEnemyHeadZ = kEyeZ + kHeadEyeOffset;
    // m_iFOV is the 4:3 horizontal FOV and the cone is tested against a single point per
    // target, so both a wider aspect and a target at the screen edge fall outside fov/2.
    constexpr float kLocalFovPadDeg = 10.0f;
    constexpr float kTeamFovDeg = 65.0f; // half-cone; teammate aim is unknown
    constexpr int kBlindAlpha = 150;
    constexpr float kTraceClearFrac = 0.99f;
    constexpr float kObjTolerance = 32.0f;

    // pm_shared's hull table: 0 standing, 1 ducking, 2 point, 3 large.
    constexpr int kDuckHull = 1;
    constexpr int kPointHull = 2;

    bool IsInCone(const ncl_math::Vector3& eye, const ncl_math::Vector3& fwd, const ncl_math::Vector3& target, float cos_half)
    {
        ncl_math::Vector3 to = target - eye;
        float len = to.Length();

        if (len < 1.0f)
        {
            return true;
        }

        return to.Dot(fwd) / len >= cos_half;
    }

    float TraceFraction(
        radar::integration::Performance& performance,
        const ncl_math::Vector3& start,
        const ncl_math::Vector3& end,
        bool& solid
    )
    {
        // Player hulls are skipped so the viewer's own body and clustered teammates
        // never break the line; goldsrc::PM_STUDIO_BOX clips props by their bounding box.
        if (pmove && pmove->PM_PlayerTraceEx)
        {
            performance.AddCounter(performance::PerfCounterId::RadarVisibilityTraces, 1);
            goldsrc::pmtrace_t trace = pmove->PM_PlayerTraceEx(
                const_cast<float*>(start.data()),
                const_cast<float*>(end.data()),
                goldsrc::PM_GLASS_IGNORE | goldsrc::PM_STUDIO_BOX,
                [](goldsrc::physent_t* pe)
                {
                    return pe->player;
                }
            );

            solid = trace.startsolid || trace.allsolid;

            return trace.fraction;
        }

        goldsrc::pmtrace_t trace{};
        trace.fraction = 1.0f;

        performance.AddCounter(performance::PerfCounterId::RadarVisibilityTraces, 1);
        gEngfuncs.pEventAPI->EV_PlayerTrace(
            const_cast<float*>(start.data()),
            const_cast<float*>(end.data()),
            goldsrc::PM_WORLD_ONLY,
            -1,
            &trace);

        solid = trace.startsolid || trace.allsolid;

        return trace.fraction;
    }

    // Tolerates an obstruction within `tolerance` units of the end point: an objective is
    // not a player hull, so the trace stops on its body. Tolerance 0 demands a clear line.
    bool IsPathClear(
        radar::integration::Performance& performance,
        const ncl_math::Vector3& start,
        const ncl_math::Vector3& end,
        float tolerance
    )
    {
        bool solid = false;
        float frac = TraceFraction(performance, start, end, solid);

        if (solid)
        {
            return false;
        }

        if (frac >= kTraceClearFrac)
        {
            return true;
        }

        float dist = (end - start).Length();

        return frac * dist >= dist - tolerance;
    }

    bool IsLocalBlind()
    {
        goldsrc::screenfade_t sf = {};
        gEngfuncs.pfnGetScreenFade(&sf);

        // Only a near-white fade (flashbang) blinds; ignore tinted map fades.
        if (!RadarFade_IsNearWhite(sf))
        {
            return false;
        }

        return RadarFade_ComputeAlpha(sf, gEngfuncs.GetClientTime()) >= kBlindAlpha;
    }
} // namespace

void RadarVisibility::Reset()
{
    for (int i = 0; i <= kRadarMaxPlayers; i++)
    {
        spotted_[i] = Spotted{};
    }

    viewer_count_ = 0;
    gating_ = false;
    last_update_ = 0.0f;
    objective_tests_open_ = false;
    traces_begun_ = false;
}

void RadarVisibility::BeginTraces()
{
    // Without EV_SetUpPlayerPrediction the physent list is not built on a HUD redraw
    // and every EV_PlayerTrace reports startsolid.
    gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(FALSE, TRUE);
    gEngfuncs.pEventAPI->EV_PushPMStates();
    gEngfuncs.pEventAPI->EV_SetTraceHull(kPointHull);
}

bool RadarVisibility::AnyViewerSees(const ncl_math::Vector3 (&target_points)[2], float time, float tolerance) const
{
    for (int i = 0; i < viewer_count_; i++)
    {
        const Viewer& viewer = viewers_[i];

        if (!IsInCone(viewer.eye, viewer.fwd, target_points[0], viewer.cos_half))
        {
            continue;
        }

        for (const ncl_math::Vector3& point : target_points)
        {
            if (!IsPathClear(performance_, viewer.eye, point, tolerance))
            {
                continue;
            }

            if (smoke_.Blocks(viewer.eye, point, time))
            {
                continue;
            }

            return true;
        }
    }

    return false;
}

void RadarVisibility::AddLocalViewer()
{
    Viewer& viewer = viewers_[viewer_count_];
    viewer.eye = ncl_math::Vector3::Ref(g_MainViewOrigin);
    ncl_math::AngleVectors(g_MainViewAngles, viewer.fwd.data(), nullptr, nullptr);
    float fov = 90.0f;

    if (gHUD && gHUD->m_iFOV && *gHUD->m_iFOV > 1)
    {
        fov = (float)*gHUD->m_iFOV;
    }

    viewer.cos_half = std::cos((fov * 0.5f + kLocalFovPadDeg) * ncl_math::kDeg2Rad);
    viewer_count_++;
}

void RadarVisibility::AddTeamViewers(const SweepContext& ctx, const RadarPolicy& policy)
{
    for (int i = 1; i <= ctx.max_clients; i++)
    {
        if (i == ctx.self_player_id)
        {
            continue;
        }

        goldsrc::cstrike::extra_player_info_t& ex = ctx.extra[i];

        if (ex.dead || RadarTeam_From(ex.teamnumber) != ctx.my_team)
        {
            continue;
        }

        // A teammate the server stopped revealing must not lend us its eyes.
        if (!policy.IsAllyRevealed(i))
        {
            continue;
        }

        goldsrc::cl_entity_t* ent = gEngfuncs.GetEntityByIndex(i);

        if (!ent || !ent->player || ent->curstate.messagenum != ctx.msgnum)
        {
            continue;
        }

        // A crouching teammate must not reveal enemies it cannot see over low cover.
        float eye_z = ent->curstate.usehull == kDuckHull ? kDuckEyeZ : kEyeZ;

        Viewer& viewer = viewers_[viewer_count_];
        viewer.eye = RadarFrame_GetPlayerPosition(*ent);
        viewer.eye.z += eye_z;

        float yaw = ent->angles[1] * ncl_math::kDeg2Rad;
        viewer.fwd = {std::cos(yaw), std::sin(yaw), 0.0f};
        viewer.cos_half = std::cos(kTeamFovDeg * ncl_math::kDeg2Rad);

        viewer_count_++;
    }
}

void RadarVisibility::SpotEnemies(const SweepContext& ctx, const RadarPolicy& policy, float time)
{
    for (int i = 1; i <= ctx.max_clients; i++)
    {
        goldsrc::cstrike::extra_player_info_t& ex = ctx.extra[i];

        if (i == ctx.self_player_id || ex.dead)
        {
            continue;
        }

        RadarTeam team = RadarTeam_From(ex.teamnumber);

        if (team != RadarTeam::Terrorist && team != RadarTeam::CounterTerrorist)
        {
            continue;
        }

        if (team == ctx.my_team && policy.IsAllyRevealed(i))
        {
            continue;
        }

        goldsrc::cl_entity_t* ent = gEngfuncs.GetEntityByIndex(i);

        if (!ent || !ent->player || ent->curstate.messagenum != ctx.msgnum)
        {
            continue;
        }

        ncl_math::Vector3 target_points[2];
        target_points[0] = RadarFrame_GetPlayerPosition(*ent);
        target_points[1] = target_points[0];
        target_points[1].z += ent->curstate.usehull == kDuckHull ? kDuckEyeZ + kHeadEyeOffset : kEnemyHeadZ;

        if (AnyViewerSees(target_points, time, 0.0f))
        {
            Spotted& spotted = spotted_[i];
            spotted.pos = target_points[0];
            spotted.yaw = ent->angles[1];
            spotted.visible = true;
        }
    }
}

void RadarVisibility::Update(int self_player_id, RadarTeam my_team, bool spectating, float time, const RadarPolicy& policy)
{
    performance::PerformanceCpuScope scope(performance_.get_monitor(), performance::PerfStage::RadarVisibility);

    if (spectating || my_team == RadarTeam::Unassigned)
    {
        Reset();
        return;
    }

    gating_ = true;

    // The throttle sits above the viewer build: viewers_ must persist between ticks
    // because CheckPointVisible reads it every frame.
    if (time >= last_update_ && time - last_update_ < kVisInterval)
    {
        return;
    }

    last_update_ = time;
    spotted_.fill(Spotted{});

    goldsrc::cl_entity_t* local = gEngfuncs.GetLocalPlayer();

    if (!local)
    {
        return;
    }

    SweepContext sweep_context;
    sweep_context.self_player_id = self_player_id;
    sweep_context.my_team = my_team;
    // The client player arrays are sized for 64; the engine caps maxclients at 32.
    sweep_context.max_clients = std::min<int>(kRadarMaxPlayers, gEngfuncs.GetMaxClients());
    sweep_context.msgnum = eng()->client_state->parsecount;
    sweep_context.extra = client()->g_PlayerExtraInfo;

    // Reset here, not in AddLocalViewer: a blinded frame skips that call and the array
    // would grow without bound.
    viewer_count_ = 0;

    if (!IsLocalBlind())
    {
        AddLocalViewer();
    }

    AddTeamViewers(sweep_context, policy);

    BeginTraces();
    SpotEnemies(sweep_context, policy, time);
    gEngfuncs.pEventAPI->EV_PopPMStates();
}

bool RadarVisibility::IsEnemyVisible(int player_id, ncl_math::Vector3& out_pos, float& out_yaw) const
{
    if (player_id < 1 || player_id > kRadarMaxPlayers || !spotted_[player_id].visible)
    {
        return false;
    }

    out_pos = spotted_[player_id].pos;
    out_yaw = spotted_[player_id].yaw;
    return true;
}

void RadarVisibility::BeginObjectiveTests()
{
    // Arm only: the PM setup is deferred to the first CheckPointVisible.
    objective_tests_open_ = true;
}

void RadarVisibility::EndObjectiveTests()
{
    performance::PerformanceCpuScope scope(performance_.get_monitor(), performance::PerfStage::RadarVisibility);

    objective_tests_open_ = false;

    if (!traces_begun_)
    {
        return;
    }

    gEngfuncs.pEventAPI->EV_PopPMStates();
    traces_begun_ = false;
}

bool RadarVisibility::CheckPointVisible(const ncl_math::Vector3& origin, float time)
{
    performance::PerformanceCpuScope scope(performance_.get_monitor(), performance::PerfStage::RadarVisibility);

    if (viewer_count_ == 0)
    {
        return false;
    }

    ncl_math::Vector3 target_points[2];
    target_points[0] = origin;
    target_points[1] = origin;
    target_points[1].z += kEnemyHeadZ;

    // Inside a batch the trace context opens on first use and stays open until
    // EndObjectiveTests; outside one it lasts only for this call.
    bool own_setup = !objective_tests_open_ && !traces_begun_;

    if (!traces_begun_)
    {
        BeginTraces();
        traces_begun_ = !own_setup;
    }

    bool visible = AnyViewerSees(target_points, time, kObjTolerance);

    if (own_setup)
    {
        gEngfuncs.pEventAPI->EV_PopPMStates();
    }

    return visible;
}
