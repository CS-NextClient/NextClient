#include "Visibility.h"
#include "SmokeRegistry.h"
#include "RadarFade.h"
#include "RadarPolicy.h"
#include "RadarUtil.h"
#include "../../main.h"
#include "../../view.h"

#include <ncl_math/vec3.h>
#include <pm_defs.h>
#include <pm_shared.h>
#include <pmtrace.h>
#include <event_api.h>
#include <screenfade.h>

#include <chrono>
#include <cmath>
#include <cstring>

namespace
{
    constexpr float kVisInterval = 0.05f; // visibility recompute period (20 Hz)
    constexpr float kSpotMemory = 1.2f; // keep a lost enemy on radar this long
    constexpr float kSpotHold = 0.2f; // full opacity window before the fade starts
    constexpr float kEyeZ = PM_VEC_VIEW; // standing view offset (pm_shared.h)
    constexpr float kDuckEyeZ = PM_VEC_DUCK_VIEW; // ducking view offset (pm_shared.h)
    constexpr float kEnemyHeadZ = 22.0f; // second trace target above the origin
    constexpr float kLocalFovPad = 10.0f; // peripheral allowance for the local view
    constexpr float kTeamFovDeg = 65.0f; // half-cone for teammates (aim unknown)
    constexpr int kBlindAlpha = 150; // white screen fade alpha that counts as blind
    constexpr float kTraceClearFrac = 0.99f; // trace fraction at/above which a sight line is unobstructed
    constexpr float kObjMargin = 32.0f; // tolerate hitting the objective's own body

    bool InCone(const ncl_math::Vector3& eye, const ncl_math::Vector3& fwd, const ncl_math::Vector3& target, float cos_half)
    {
        ncl_math::Vector3 to = target - eye;
        float len = to.Length();

        if (len < 1.0f)
        {
            return true;
        }

        bool result = to.Dot(fwd) / len >= cos_half;
        return result;
    }

    int IgnorePlayerEnts(physent_t* pe)
    {
        return pe->player; // skip every player hull, the observer included
    }

    float TraceFraction(const ncl_math::Vector3& start, const ncl_math::Vector3& end, bool* solid)
    {
        // World, brush entities (doors) and solid studio props block sight; player
        // hulls do not (the ignore callback skips them) so clustered teammates and
        // the observer's own body never break the line. Glass is skipped by its
        // render mode. PM_STUDIO_BOX clips props by their bounding box.
        if (pmove && pmove->PM_PlayerTraceEx)
        {
            pmtrace_t tr = pmove->PM_PlayerTraceEx(
                const_cast<float*>(start.data()), const_cast<float*>(end.data()), PM_GLASS_IGNORE | PM_STUDIO_BOX, IgnorePlayerEnts
            );

            if (solid)
            {
                *solid = tr.startsolid || tr.allsolid;
            }

            return tr.fraction;
        }

        // Fallback when the extended trace is unavailable: world only (players
        // cannot block it either, but props and doors will not block).
        pmtrace_t tr;
        tr.fraction = 1.0f;
        tr.startsolid = 0;
        tr.allsolid = 0;
        gEngfuncs.pEventAPI->EV_PlayerTrace(const_cast<float*>(start.data()), const_cast<float*>(end.data()), PM_WORLD_ONLY, -1, &tr);

        if (solid)
        {
            *solid = tr.startsolid || tr.allsolid;
        }

        return tr.fraction;
    }

    // Clear sight line, tolerant of an obstruction within `margin` units of the
    // end point: an objective entity (hostage, bomb) is not a player hull, so the
    // line of sight to it stops on its own body. Treat the line as clear when it
    // reaches that close to the target. Margin 0 demands a fully clear line.
    bool TraceReaches(const ncl_math::Vector3& start, const ncl_math::Vector3& end, float margin)
    {
        bool solid = false;
        float frac = TraceFraction(start, end, &solid);

        if (solid)
        {
            return false;
        }

        if (frac >= kTraceClearFrac)
        {
            return true;
        }

        float dist = (end - start).Length();

        bool result = frac * dist >= dist - margin;
        return result;
    }

    bool LocalBlind()
    {
        screenfade_t sf = {};
        gEngfuncs.pfnGetScreenFade(&sf);

        // Only a near-white fade (flashbang) blinds; ignore tinted map fades.
        if (!RadarFade::NearWhite(sf))
        {
            return false;
        }

        bool result = RadarFade::Alpha(sf, gEngfuncs.GetClientTime()) >= kBlindAlpha;
        return result;
    }

    // Adds the scope's wall time to the given counter (see traces_ms).
    struct ScopedMsCounter
    {
        float& acc;
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

        ~ScopedMsCounter()
        {
            acc += std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
    };
} // namespace

void RadarVisibility::Reset()
{
    for (int i = 0; i <= kMaxPlayers; i++)
    {
        spotted_[i] = Spotted{};
    }

    RadarSmoke::Reset();
    nviewers_ = 0;
    last_update_ = 0.0f;
    batch_armed_ = false;
    trace_open_ = false;
}

void RadarVisibility::BeginTraces()
{
    // Prepare the trace context the same way the SDK does for bullet traces;
    // without EV_SetUpPlayerPrediction the physent list is not built here (HUD
    // redraw) and every EV_PlayerTrace reports startsolid.
    gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
    gEngfuncs.pEventAPI->EV_PushPMStates();
    gEngfuncs.pEventAPI->EV_SetTraceHull(2);
}

bool RadarVisibility::AnyViewerSees(const ncl_math::Vector3 (&pts)[2], float time, float margin) const
{
    for (int vi = 0; vi < nviewers_; vi++)
    {
        const Viewer& v = viewers_[vi];

        if (!InCone(v.eye, v.fwd, pts[0], v.cos_half))
        {
            continue;
        }

        for (int p = 0; p < 2; p++)
        {
            if (!TraceReaches(v.eye, pts[p], margin))
            {
                continue;
            }

            if (RadarSmoke::Blocks(v.eye, pts[p], time))
            {
                continue;
            }

            return true;
        }
    }

    return false;
}

void RadarVisibility::Update(int self_idx, int my_team, bool spectating, float time, const RadarPolicy& policy)
{
    static_assert(kMaxPlayers == MAX_PLAYERS, "player table size mismatch");

    ScopedMsCounter counter{traces_ms_};

    if (spectating || my_team == 0)
    {
        gating_ = false;
        nviewers_ = 0;
        return;
    }

    gating_ = true;

    if (time >= last_update_ && time - last_update_ < kVisInterval)
    {
        return;
    }

    last_update_ = time;

    cl_entity_t* local = gEngfuncs.GetLocalPlayer();

    if (!local)
    {
        return;
    }

    int msgnum = local->curstate.messagenum;
    extra_player_info_t* extra = client()->g_PlayerExtraInfo;

    // The engine caps maxclients at 32; the client arrays are sized for 64, but the
    // slots past maxclients are always empty, so do not walk them.
    int max_clients = std::min<int>(kMaxPlayers, gEngfuncs.GetMaxClients());

    nviewers_ = 0;

    bool blind = LocalBlind();

    if (!blind)
    {
        Viewer& v = viewers_[nviewers_];
        v.eye = ncl_math::Vector3::Ref(g_MainViewOrigin);
        ncl_math::AngleVectors(g_MainViewAngles, v.fwd.data(), nullptr, nullptr);
        float fov = 90.0f;

        if (gHUD && gHUD->m_iFOV && *gHUD->m_iFOV > 1)
        {
            fov = (float)*gHUD->m_iFOV;
        }

        v.cos_half = std::cos((fov * 0.5f + kLocalFovPad) * ncl_math::kDeg2Rad);
        nviewers_++;
    }

    for (int j = 1; j <= max_clients; j++)
    {
        if (j == self_idx)
        {
            continue;
        }

        extra_player_info_t& e = extra[j];

        if (e.dead || e.teamnumber != my_team)
        {
            continue;
        }

        // A teammate the server stopped revealing is an opponent in this mode, so it
        // must not lend its eyes to us.
        if (!policy.AllyRevealed(j))
        {
            continue;
        }

        cl_entity_t* ent = gEngfuncs.GetEntityByIndex(j);

        if (!ent || !ent->player || ent->curstate.messagenum != msgnum)
        {
            continue;
        }

        // usehull 1 is the ducked hull (as in view.cpp); a crouching teammate looks
        // from lower down, so use the ducking view offset instead of the standing one
        // to avoid revealing enemies the teammate cannot see over low cover.
        float eye_z = ent->curstate.usehull == 1 ? kDuckEyeZ : kEyeZ;
        Viewer& v = viewers_[nviewers_];
        v.eye = {ent->origin[0], ent->origin[1], ent->origin[2] + eye_z};
        float yaw = ent->angles[1] * ncl_math::kDeg2Rad;
        v.fwd = {std::cos(yaw), std::sin(yaw), 0.0f};
        v.cos_half = std::cos(kTeamFovDeg * ncl_math::kDeg2Rad);
        nviewers_++;
    }

    BeginTraces();

    for (int e = 1; e <= max_clients; e++)
    {
        extra_player_info_t& ex = extra[e];

        if (e == self_idx || ex.dead)
        {
            continue;
        }

        int team = ex.teamnumber;

        if (team != TEAM_TERRORIST && team != TEAM_CT)
        {
            continue;
        }

        // Same team, still revealed by the server: a real teammate, drawn from its
        // broadcast rather than spotted. Otherwise it is an opponent here and takes the
        // line-of-sight path with the rest of them.
        if (team == my_team && policy.AllyRevealed(e))
        {
            continue;
        }

        cl_entity_t* ent = gEngfuncs.GetEntityByIndex(e);

        if (!ent || !ent->player || ent->curstate.messagenum != msgnum)
        {
            continue;
        }

        ncl_math::Vector3 pts[2];
        pts[0] = ncl_math::Vector3::Ref(ent->origin);
        pts[1] = pts[0];
        pts[1].z += kEnemyHeadZ;

        if (AnyViewerSees(pts, time, 0.0f))
        {
            Spotted& s = spotted_[e];
            s.last_seen = time;
            s.pos = {ent->origin[0], ent->origin[1]};
            s.yaw = ent->angles[1];
            s.valid = true;
        }
    }

    gEngfuncs.pEventAPI->EV_PopPMStates();
}

bool RadarVisibility::ShouldShowEnemy(int idx, float time, ncl_math::Vector2* out_pos, float* out_yaw, float* out_alpha) const
{
    if (idx < 1 || idx > kMaxPlayers)
    {
        return false;
    }

    const Spotted& s = spotted_[idx];
    float age = time - s.last_seen;

    if (!s.valid || age > kSpotMemory)
    {
        return false;
    }

    float a = ncl_math::HoldLinear(age, kSpotHold, kSpotMemory);

    *out_pos = s.pos;
    *out_yaw = s.yaw;
    *out_alpha = a;

    return true;
}

void RadarVisibility::BeginObjectiveTests()
{
    // Arm only: the PM setup (a physent-list rebuild) is deferred to the first
    // PointVisible, so frames whose objective tests are all throttled away pay nothing.
    batch_armed_ = true;
}

void RadarVisibility::EndObjectiveTests()
{
    ScopedMsCounter counter{traces_ms_};

    batch_armed_ = false;

    if (!trace_open_)
    {
        return;
    }

    gEngfuncs.pEventAPI->EV_PopPMStates();
    trace_open_ = false;
}

bool RadarVisibility::PointVisible(const ncl_math::Vector3& origin, float time)
{
    ScopedMsCounter counter{traces_ms_};

    if (nviewers_ == 0)
    {
        return false;
    }

    ncl_math::Vector3 pts[2];
    pts[0] = origin;
    pts[1] = origin;
    pts[1].z += kEnemyHeadZ;

    // Inside a batch the trace context opens on first use and stays open until
    // EndObjectiveTests; outside one it is set up just for this call.
    bool own_setup = !batch_armed_ && !trace_open_;

    if (!trace_open_)
    {
        BeginTraces();
        trace_open_ = !own_setup;
    }

    bool visible = AnyViewerSees(pts, time, kObjMargin);

    if (own_setup)
    {
        gEngfuncs.pEventAPI->EV_PopPMStates();
    }

    return visible;
}
