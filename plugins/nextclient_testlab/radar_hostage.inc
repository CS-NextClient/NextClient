#include "hostage_follow.inc"

bool:Hostage_Prepare(ScenarioId:scenario, operator_id, generation)
{
    new const names[][] = {"hostage_start", "ct_use", "viewer_open"};
    for (new index = 0; index < sizeof names; index++)
    {
        if (!Radar_RequireAnchor(names[index], index == 0 ? Anchor_Hostage : Anchor_Player)) { return false; }
    }
    if (!Radar_SaveHostages(false, false)) { return false; }
    g_SceneHostage = Radar_FindClass("hostage_entity");
    g_SceneHostageId = Radar_HostageId(g_SceneHostage);
    g_SceneRescue = scenario == Scenario_HostageEnd && equal(g_Outcome, "rescue");
    if (g_SceneRescue)
    {
        if (!Radar_RequireAnchor("rescue_hostage_start", Anchor_Hostage) || !Radar_RequireAnchor("rescue_ct_use", Anchor_Player)
            || !Radar_RequireAnchor("rescue_entry", Anchor_Player)) { return false; }
    }
    if (scenario == Scenario_HostageFollow)
    {
        new const routes[][] = {"hostage_follow_out", "hostage_return"};
        for (new index = 0; index < sizeof routes; index++)
        {
            new route = Maps_FindRoute(routes[index]);
            if (route < 0 || Maps_FindView("open") < 0)
            {
                Core_Fail(generation, "setup_failed_follow_profile; calibrate hostage_follow_out and hostage_return");
                return false;
            }
            for (new point = 0; point < g_RouteLengths[route]; point++)
            {
                new anchor = g_RouteAnchors[route][point];
                if (!Maps_CheckPlayerAnchor(anchor) || (point && !Geometry_ClearSegment(
                    g_AnchorOrigins[g_RouteAnchors[route][point - 1]], g_AnchorOrigins[anchor])))
                {
                    new code[192];
                    formatex(code, charsmax(code), "setup_failed_follow_route_hull route=%s anchor=%s previous=%s hull=%d", routes[index],
                        g_AnchorNames[anchor], point ? g_AnchorNames[g_RouteAnchors[route][point - 1]] : "start", Maps_CheckPlayerAnchor(anchor));
                    Core_Fail(generation, code);
                    return false;
                }
            }
        }
    }
    if ((scenario == Scenario_HostageStatic || scenario == Scenario_HostageVisibility)
        && (Maps_FindView("open") < 0 || Maps_FindView("occluded") < 0 || (scenario == Scenario_HostageVisibility && Maps_FindView("outside") < 0)))
    {
        Core_Fail(generation, "setup_failed_views; calibrate viewer_open viewer_occluded viewer_outside");
        return false;
    }
    if (!Actors_Create(0, TEAM_TERRORIST) || !Actors_Create(1, TEAM_CT)) { return false; }
    new TeamName:team = g_OperatorRole == 1 ? TEAM_CT : g_OperatorRole == 2 ? TEAM_TERRORIST : TEAM_SPECTATOR;
    if (get_member(operator_id, m_iJoiningState) != JOINED || !Actors_EnsureTeam(operator_id, team)) { return false; }
    if (team != TEAM_SPECTATOR) { rg_round_respawn(operator_id); }
    g_HostageInitialMessages = 0;
    g_HostageMoveMessages = 0;
    g_HostageEndMessages = 0;
    g_BadMessageAudience = 0;
    return Core_SetCvar("hostage_ai_enable", "0") && Core_SetCvar("mp_hostage_hurtable", "1");
}

bool:Hostage_Place()
{
    new anchor = Maps_FindAnchor(g_SceneRescue ? "rescue_hostage_start" : "hostage_start");
    new Float:zero[3];
    set_pev(g_SceneHostage, pev_velocity, zero);
    engfunc(EngFunc_SetOrigin, g_SceneHostage, g_AnchorOrigins[anchor]);
    if (!Radar_PlaceActor(1, g_SceneRescue ? "rescue_ct_use" : "ct_use")) { return false; }
    if (g_SceneRescue)
    {
        new Float:view[3];
        anchor = Maps_FindAnchor("rescue_entry");
        if (!Actors_FindNear(g_AnchorOrigins[anchor], view, 128.0)) { return false; }
        if (g_HeadlessOperator) { Actors_Place(3, view); }
        else { engfunc(EngFunc_SetOrigin, g_OperatorSlot, view); }
        return true;
    }
    return Radar_PlaceOperator("viewer_open");
}

bool:Hostage_StartStep(ScenarioId:scenario, step_index, generation)
{
    #pragma unused generation
    g_ActionStarted = get_gametime();
    if (scenario == Scenario_HostageStatic || scenario == Scenario_HostageVisibility)
    {
        new const views[][] = {"open", "occluded", "outside", "open"};
        if (!Radar_StartView(views[step_index], g_SceneHostage)) { return false; }
        g_ScenePhase = 10;
        g_CoreDeadline = get_gametime() + 6.0;
        Core_Reply(g_OperatorSlot, step_index == 0 ? "open: inspect one marker and team policy" : "visibility: inspect marker clearing after two seconds");
        return true;
    }
    if (scenario == Scenario_HostageFollow)
    {
        if (step_index == 0 || step_index == 3)
        {
            g_ScenePhase = 20;
            g_CoreDeadline = get_gametime() + 5.0;
        }
        else
        {
            if (!Radar_StartRoute(step_index == 1 ? "hostage_follow_out" : "hostage_return")) { return false; }
            g_FollowMessagesBefore = g_FollowOperatorMessages;
            g_FollowInsideSince = 0.0;
            g_ScenePhase = 30;
            g_CoreDeadline = get_gametime() + float(g_RouteLengths[g_SceneRoute]) * g_RouteTimeouts[g_SceneRoute] + 10.0;
        }
        new const instructions[][] = {"use: CT leads the hostage; inspect one marker inside PVS",
            "outbound: fixed viewer; wait for moving hostage outside PVS and three HostagePos updates",
            "return: moving hostage returns to fixed viewer PVS; inspect source handoff", "stop: CT uses the hostage again"};
        Core_Reply(g_OperatorSlot, instructions[step_index]);
        return true;
    }
    if (scenario == Scenario_HostageEnd)
    {
        g_SceneMessageBefore = g_HostageEndMessages;
        g_SceneRescuedBefore = get_member_game(m_iHostagesRescued);
        if (g_SceneRescue)
        {
            g_ScenePhase = 20;
            g_CoreDeadline = get_gametime() + 45.0;
        }
        else
        {
            ExecuteHamB(Ham_TakeDamage, g_SceneHostage, g_ActorSlots[1], g_ActorSlots[1], 10000.0, DMG_GENERIC);
            g_ScenePhase = 40;
            g_CoreDeadline = get_gametime() + 5.0;
        }
        return true;
    }
    return false;
}

Hostage_Tick(ScenarioId:scenario, generation, Float:now)
{
    if (!pev_valid(g_SceneHostage) || (g_SceneHostageId > 0 && Radar_HostageId(g_SceneHostage) != g_SceneHostageId))
    {
        Core_Fail(generation, "setup_failed_hostage_identity");
        return;
    }
    if (g_CoreState == Core_Preparing)
    {
        if (g_ScenePhase == 0 && Actors_AreJoined())
        {
            if (!Radar_SaveHostages(false)) { return; }
            g_SceneHostageId = Radar_HostageId(g_SceneHostage);
            if (g_OperatorRole == 4)
            {
                if (!(get_member(g_OperatorSlot, m_afPhysicsFlags) & PFLAG_OBSERVER)) { return; }
                rg_set_observer_mode(g_OperatorSlot, OBS_ROAMING);
            }
            if (!Hostage_Place()) { Core_Fail(generation, "setup_failed_hostage_placement"); return; }
            if (scenario == Scenario_HostageFollow)
            {
                if (!Diagnostics_StartView("open", g_SceneHostage)) { Core_Fail(generation, "setup_failed_follow_initial_view"); return; }
                pev(g_OperatorSlot, pev_origin, g_FollowViewerOrigin);
                if (!g_HeadlessOperator) { set_pev(g_OperatorSlot, pev_movetype, MOVETYPE_NONE); }
            }
            g_ScenePhase = 1;
            g_ActionStarted = now;
        }
        else if (g_ScenePhase == 1 && now - g_ActionStarted > 1.1 && g_HostageInitialMessages > 0)
        {
            Radar_ProbeCheck("hostage_baseline", Radar_SaveHostages(true), "existing hostage_id sequence retained after preparation");
            Radar_ProbeCheck("hostage_initial_message", true, "HostagePos flag 1 observed separately from periodic flag 0");
            Core_StageReady(generation);
        }
        return;
    }
    if (g_CoreState != Core_Running) { return; }
    if (scenario == Scenario_HostageFollow)
    {
        Hostage_FollowTick(now);
        if (g_CoreState != Core_Running) { return; }
    }
    if (g_BadMessageAudience)
    {
        Core_Fail(generation, "hostage_message_audience_failed");
        return;
    }
    if (g_ScenePhase == 10)
    {
        new result = Diagnostics_ViewResult();
        if (result < 0) { Core_Fail(generation, "setup_failed_pvs_unknown_or_mismatch; calibrate current viewer anchor"); }
        else if (result > 0 && now - g_ActionStarted >= 2.0)
        {
            Radar_ProbeCheck("hostage_view_los", true, "live LOS matches profile; PVS is independently recorded");
            Core_StageFinished(generation);
        }
    }
    else if (g_ScenePhase == 20)
    {
        new Float:hostage[3], Float:actor[3];
        pev(g_SceneHostage, pev_origin, hostage);
        pev(g_ActorSlots[1], pev_origin, actor);
        if (get_distance_f(hostage, actor) > 63.0)
        {
            Radar_MoveToward(1, hostage, 80.0);
            return;
        }
        g_ActorForward[1] = 0.0;
        Radar_AimUse(1, g_SceneHostage);
        new Float:next_attack;
        next_attack = Float:get_member(g_ActorSlots[1], m_flNextAttack);
        if (next_attack > 0.0 || !(pev(g_ActorSlots[1], pev_flags) & FL_ONGROUND)) { return; }
        g_ActorButtons[1] = IN_USE;
        g_UseFrames = 0;
        g_ActionStarted = now;
        g_ScenePhase = 21;
    }
    else if (g_ScenePhase == 21)
    {
        new follower = cs_get_hostage_foll(g_SceneHostage);
        new bool:stopping = scenario == Scenario_HostageFollow && g_Stage == 3;
        if (stopping ? follower == 0 : follower == g_ActorSlots[1])
        {
            Actors_ReleaseInput();
            Radar_ProbeCheck(stopping ? "hostage_stop" : "hostage_use", true, "normal IN_USE changed the selected hostage follower");
            if (g_SceneRescue)
            {
                new anchor = Maps_FindAnchor("rescue_entry"), Float:origin[3];
                for (new axis = 0; axis < 3; axis++) { g_SceneTarget[axis] = g_AnchorOrigins[anchor][axis]; }
                pev(g_ActorSlots[1], pev_origin, origin);
                if (!Geometry_ClearSegment(origin, g_SceneTarget)) { Core_Fail(generation, "setup_failed_rescue_path"); return; }
                g_ScenePhase = 50;
            }
            else if (scenario == Scenario_HostageFollow && !stopping) { g_ScenePhase = 22; }
            else { Core_StageFinished(generation); }
        }
    }
    else if (g_ScenePhase == 22)
    {
        if (Hostage_FollowGate(0)) { Core_StageFinished(generation); }
    }
    else if (g_ScenePhase == 30)
    {
        new result = Radar_RouteTick(1, now);
        if (result < 0) { Core_Fail(generation, "setup_failed_route_stuck_or_timeout"); }
        else if (result > 0) { g_ScenePhase = 31; g_CoreDeadline = now + 5.0; }
    }
    else if (g_ScenePhase == 31)
    {
        new Float:origin[3], Float:leader[3], Float:minimum[3];
        pev(g_SceneHostage, pev_origin, origin);
        pev(g_ActorSlots[1], pev_origin, leader);
        pev(g_ActorSlots[1], pev_mins, minimum);
        if (Hostage_FollowLeaderDistance(origin, leader, minimum[2]) < 80.0
            && cs_get_hostage_foll(g_SceneHostage) == g_ActorSlots[1]
            && g_FollowOperatorMessages > g_FollowMessagesBefore && Hostage_FollowGate(g_Stage))
        {
            Radar_ProbeCheck(g_Stage == 1 ? "hostage_route" : "hostage_return_route", true,
                "CT traversed the route; selected hostage follows within 80 units of leader feet; HostagePos reached the operator");
            Core_StageFinished(generation);
        }
    }
    else if (g_ScenePhase == 40)
    {
        if (pev(g_SceneHostage, pev_deadflag) != DEAD_NO && g_HostageEndMessages > g_SceneMessageBefore)
        {
            Radar_ProbeCheck("hostage_killed", true, "damage/killed path and HostageK for the same hostage_id");
            Core_StageFinished(generation);
        }
    }
    else if (g_ScenePhase == 50 || g_ScenePhase == 51)
    {
        new Float:origin[3];
        pev(g_ActorSlots[1], pev_origin, origin);
        if (g_ScenePhase == 50)
        {
            if (get_distance_f(origin, g_SceneTarget) <= 16.0)
            {
                Actors_ReleaseInput();
                g_ScenePhase = 51;
            }
            else { Radar_MoveToward(1, g_SceneTarget); }
        }
        if (get_member_game(m_iHostagesRescued) > g_SceneRescuedBefore && (pev(g_SceneHostage, pev_effects) & EF_NODRAW)
            && g_HostageEndMessages > g_SceneMessageBefore)
        {
            Radar_ProbeCheck("hostage_rescued", true, "normal follow/rescue increased GameDLL counter and sent HostageK");
            Core_StageFinished(generation);
        }
    }
}
