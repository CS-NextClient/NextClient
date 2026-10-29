new g_SelftestCoreStep;
new g_SelftestCoreIteration;
new Float:g_SelftestDeadline;
new g_SceneTestStep;
new g_SceneTestCase;
new g_SceneTestIteration;
new g_SceneTestFailures;
new g_SceneTestEndCase;
new g_SceneTestSuite[16];
new g_SceneTestRepeats;
new g_ReviewStep;
new bool:g_ReviewPassed;
new g_ReviewFailedRun[64];
new Float:g_ReviewNext;
new const ScenarioId:kHostageCases[] = {Scenario_HostageStatic, Scenario_HostageStatic, Scenario_HostageFollow,
    Scenario_HostageVisibility, Scenario_HostageVisibility, Scenario_HostageVisibility, Scenario_HostageEnd, Scenario_HostageEnd};
new const kHostageRoles[] = {1, 2, 1, 1, 2, 4, 1, 1};
new const ScenarioId:kBombCases[] = {Scenario_BombCycle, Scenario_BombCycle, Scenario_BombCycle, Scenario_BombCycle,
    Scenario_BombVisibility, Scenario_BombVisibility, Scenario_BombVisibility, Scenario_BombVisibility, Scenario_BombObserver};
new const kBombRoles[] = {2, 1, 2, 1, 2, 1, 2, 1, 2};

Selftests_CaseCount()
{
    return equal(g_SceneTestSuite, "hostage") ? sizeof kHostageCases : equal(g_SceneTestSuite, "bomb") ? sizeof kBombCases
        : equal(g_SceneTestSuite, "players") ? 2 : 3;
}

ScenarioId:Selftests_Scenario()
{
    return equal(g_SceneTestSuite, "hostage") ? kHostageCases[g_SceneTestCase] : equal(g_SceneTestSuite, "bomb") ? kBombCases[g_SceneTestCase]
        : equal(g_SceneTestSuite, "players") ? Scenario_Players : Scenario_Lifecycle;
}

Selftests_Role()
{
    return equal(g_SceneTestSuite, "hostage") ? kHostageRoles[g_SceneTestCase] : equal(g_SceneTestSuite, "bomb") ? kBombRoles[g_SceneTestCase]
        : g_SceneTestCase == 0 ? 1 : g_SceneTestCase == 1 ? 2 : 4;
}

public Selftests_Command()
{
    new suite[16];
    read_argv(1, suite, charsmax(suite));
    if (!equal(suite, "core") && !equal(suite, "hostage") && !equal(suite, "bomb") && !equal(suite, "players") && !equal(suite, "lifecycle") && !equal(suite, "review"))
    {
        server_print("Usage: nct_selftest core|hostage|bomb|players|lifecycle|review [case]");
        return PLUGIN_HANDLED;
    }
    if (g_SelftestCoreStep || g_SceneTestStep || g_ReviewStep || Core_IsCallbackCurrent(g_Generation) || g_Resetting || g_Probe != Probe_None || Actors_Count(true))
    {
        server_print("TestLab: busy");
        return PLUGIN_HANDLED;
    }
    g_RunId[0] = 0;
    g_Stage = -1;
    if (equal(suite, "review"))
    {
        new map_name[32];
        get_mapname(map_name, charsmax(map_name));
        if (!equal(map_name, "cs_office") || !Actors_Create(3, TEAM_CT))
        {
            Radar_ProbeCheck("selftest_finished", false, "review suite requires cs_office and a technical operator");
            return PLUGIN_HANDLED;
        }
        g_ReviewPassed = true;
        g_ReviewStep = 1;
        g_SelftestDeadline = get_gametime() + 30.0;
    }
    else if (equal(suite, "core")) { Selftests_Core(); }
    else
    {
        g_SceneTestCase = 0;
        g_SceneTestRepeats = 5;
        copy(g_SceneTestSuite, charsmax(g_SceneTestSuite), suite);
        g_SceneTestEndCase = Selftests_CaseCount();
        if (read_argc() > 2)
        {
            new selected[8];
            read_argv(2, selected, charsmax(selected));
            g_SceneTestCase = str_to_num(selected);
            if (g_SceneTestCase < -1 || g_SceneTestCase >= Selftests_CaseCount())
            {
                server_print("TestLab: case index outside suite");
                return PLUGIN_HANDLED;
            }
            if (g_SceneTestCase == -1) { g_SceneTestCase = 0; }
            else { g_SceneTestEndCase = g_SceneTestCase + 1; }
        }
        if (read_argc() > 3)
        {
            new selected[8];
            read_argv(3, selected, charsmax(selected));
            g_SceneTestRepeats = str_to_num(selected);
            if (g_SceneTestRepeats < 1 || g_SceneTestRepeats > 5) { server_print("TestLab: repeats must be 1..5"); return PLUGIN_HANDLED; }
        }
        g_SceneTestIteration = 0;
        g_SceneTestFailures = 0;
        if (Actors_Create(3, TEAM_CT))
        {
            g_SceneTestStep = 1;
            g_SelftestDeadline = get_gametime() + (equal(suite, "bomb") ? 1200.0 : 600.0);
            Diagnostics_SetTrace(true);
        }
        else { Radar_ProbeCheck("selftest_finished", false, "cannot create technical operator"); }
    }
    return PLUGIN_HANDLED;
}

Selftests_Core()
{
    Radar_ProbeCheck("follow_fresh_inside", Hostage_FollowVisibilityMatches(true, 1, true, 0.1), "fresh carried entity accepted inside PVS");
    Radar_ProbeCheck("follow_hidden_pack_inside", !Hostage_FollowVisibilityMatches(true, 0, true, 0.1), "inside PVS requires original_pack=1");
    Radar_ProbeCheck("follow_packed_outside", !Hostage_FollowVisibilityMatches(false, 1, false, 0.1), "outside PVS requires original_pack=0");
    Radar_ProbeCheck("follow_stale_visibility", !Hostage_FollowVisibilityMatches(false, 0, false, 0.6), "stale callback rejected");
    Radar_ProbeCheck("follow_outside_short", !Hostage_FollowOutsideReady(5.9, 3, 256.0), "outside observation must last six seconds");
    Radar_ProbeCheck("follow_outside_messages", !Hostage_FollowOutsideReady(6.0, 2, 256.0), "three selected messages required");
    Radar_ProbeCheck("follow_outside_motion", !Hostage_FollowOutsideReady(6.0, 3, 255.0), "256 units of reported motion required");
    Radar_ProbeCheck("follow_outside_ready", Hostage_FollowOutsideReady(6.0, 3, 256.0), "outside duration, messages and motion accepted together");
    new Float:hostage[3] = {1745.2628, 722.1525, -159.968}, Float:leader[3] = {1711.7572, 782.0133, -123.96875};
    Radar_ProbeCheck("follow_natural_stop", Hostage_FollowLeaderDistance(hostage, leader, -36.0) < 80.0,
        "observed Windows return pose accepted at natural follow distance");
    hostage[0] = leader[0] + 81.0;
    hostage[1] = leader[1];
    Radar_ProbeCheck("follow_distant_leader", Hostage_FollowLeaderDistance(hostage, leader, -36.0) > 80.0,
        "hostage beyond natural stop distance rejected");
    hostage[0] = leader[0];
    hostage[2] += 100.0;
    Radar_ProbeCheck("follow_different_floor", Hostage_FollowLeaderDistance(hostage, leader, -36.0) > 80.0,
        "vertical separation retained in follow distance");
    new command[32], info[2], flags, count;
    new bool:unrestricted = true;
    for (new index = 0; index < get_concmdsnum(-1, 1); index++)
    {
        get_concmd(index, command, charsmax(command), flags, info, charsmax(info), -1, 1);
        if (equal(command, "nct_", 4))
        {
            count++;
            if (flags) { unrestricted = false; }
        }
    }
    Radar_ProbeCheck("commands_unrestricted", unrestricted && count == 23, "all 23 client commands require no AMXX flags");
    new previous_generation = g_Generation;
    g_CoreState = Core_Preparing;
    g_Generation++;
    Core_StageReady(previous_generation);
    Radar_ProbeCheck("old_callback_canceled", g_CoreState == Core_Preparing, "stale generation cannot change preparing");
    Core_StageFinished(g_Generation);
    Radar_ProbeCheck("invalid_stage_not_ready", g_CoreState == Core_Preparing, "finished callback requires running");
    g_CoreState = Core_Running;
    Radar_ProbeCheck("step_while_running_busy", !Core_CanStep(), "running rejects second step");
    new Float:remainder;
    new msec = Actors_AccumulateMsec(0.0015, remainder);
    msec += Actors_AccumulateMsec(0.0015, remainder);
    Radar_ProbeCheck("fractional_msec", msec == 3 && floatabs(remainder) < 0.001, "two 1.5 ms ticks produce 3 ms");
    Radar_ProbeCheck("gap_rejected", Actors_AccumulateMsec(0.251, remainder) == -1, "no catch-up after 0.25 seconds");
    Radar_ProbeCheck("msec_bound", Actors_AccumulateMsec(0.125, remainder) == -1, "RunPlayerMove msec never exceeds 100");
    Radar_ProbeCheck("pending_wrong_session", !Core_PendingIsValid("wrong", g_MapEpoch, true, 200, 100), "session mismatch rejected");
    Radar_ProbeCheck("pending_wrong_map", !Core_PendingIsValid(g_SessionId, g_MapEpoch, false, 200, 100), "map mismatch rejected");
    Radar_ProbeCheck("pending_expired", !Core_PendingIsValid(g_SessionId, g_MapEpoch, true, 99, 100), "expired pending rejected");
    Radar_ProbeCheck("pending_valid", Core_PendingIsValid(g_SessionId, g_MapEpoch, true, 200, 100), "same session, map epoch and unexpired request accepted");
    Core_Cancel("selftest");
    g_CoreState = Core_Idle;
    g_SelftestCoreIteration = 0;
    if (!Actors_Create(3, TEAM_CT))
    {
        Radar_ProbeCheck("selftest_finished", false, "cannot create technical operator");
        return;
    }
    g_SelftestCoreStep = 1;
    g_SelftestDeadline = get_gametime() + 30.0;
}

Selftests_Tick(Float:now)
{
    Selftests_ReviewTick(now);
    Selftests_ScenesTick(now);
    if (!g_SelftestCoreStep)
    {
        return;
    }
    if (now > g_SelftestDeadline)
    {
        Core_Cancel("selftest_timeout");
        Actors_Remove(true);
        g_SelftestCoreStep = 0;
        g_HeadlessOperator = false;
        Radar_ProbeCheck("selftest_finished", false, "core live test timed out");
        return;
    }
    if (g_SelftestCoreStep == 1 && Actors_IsJoined(3))
    {
        g_HeadlessOperator = true;
        g_OperatorSlot = g_ActorSlots[3];
        g_OperatorUserid = g_ActorUserids[3];
        g_CoreState = Core_Ready;
        Core_OnDisconnect(g_OperatorSlot, g_OperatorUserid + 1);
        Radar_ProbeCheck("reused_slot_no_role", g_CoreState == Core_Ready, "different userid cannot cancel or inherit operator");
        Core_OnDisconnect(g_OperatorSlot, g_OperatorUserid);
        Radar_ProbeCheck("operator_disconnect_cancels", g_CoreState == Core_Canceled && !g_OperatorSlot, "matching disconnect cancels current generation");
        g_OperatorSlot = g_ActorSlots[3];
        g_OperatorUserid = g_ActorUserids[3];
        g_SelftestCoreStep = 2;
    }
    else if (g_SelftestCoreStep == 2)
    {
        if (!Actors_Create(0, TEAM_TERRORIST) || !Actors_Create(1, TEAM_CT))
        {
            g_SelftestDeadline = now;
            return;
        }
        g_SelftestCoreStep = 3;
    }
    else if (g_SelftestCoreStep == 3 && Actors_AreJoined())
    {
        Core_RoundReset(false);
        g_SelftestCoreStep = 4;
    }
    else if (g_SelftestCoreStep == 4 && !g_Resetting)
    {
        new bool:clean = g_CoreState == Core_Idle && Actors_Count() == 0 && !Radar_FindBomb() && !Radar_FindBombBox();
        Radar_ProbeCheck("core_round_reset", clean, "owned actors removed, restart hook observed, objective baseline restored");
        if (!clean)
        {
            g_SelftestDeadline = now;
            return;
        }
        if (++g_SelftestCoreIteration < 5)
        {
            g_SelftestCoreStep = 2;
        }
        else
        {
            g_CoreState = Core_Ready;
            server_cmd("kick #%d", g_OperatorUserid);
            g_SelftestCoreStep = 5;
        }
    }
    else if (g_SelftestCoreStep == 5 && !Actors_IsCurrent(3))
    {
        Radar_ProbeCheck("real_disconnect_callback", g_CoreState == Core_Canceled && !g_OperatorSlot, "GameDLL disconnect cancels technical operator");
        Actors_Remove(true);
        g_HeadlessOperator = false;
        g_SelftestCoreStep = 0;
        g_CoreState = Core_Idle;
        Radar_ProbeCheck("selftest_finished", true, "five real resets; no owned actors or deferred callbacks");
    }
}

Selftests_ReviewCheck(const name[], bool:passed, const actual[])
{
    g_ReviewPassed = g_ReviewPassed && passed;
    Radar_ProbeCheck(name, passed, actual);
}

Selftests_ReviewTick(Float:now)
{
    if (!g_ReviewStep) { return; }
    if (now > g_SelftestDeadline)
    {
        Core_Cancel("review_selftest_timeout");
        Actors_Remove(true);
        g_HeadlessOperator = false;
        g_ReviewStep = 0;
        Radar_ProbeCheck("selftest_finished", false, "review regression timed out");
        return;
    }
    if (g_ReviewStep == 1 && Actors_IsJoined(3))
    {
        g_HeadlessOperator = true;
        g_OperatorSlot = g_ActorSlots[3];
        g_OperatorUserid = g_ActorUserids[3];
        Core_Begin(Scenario_HostageFollow, 1);
        copy(g_ReviewFailedRun, charsmax(g_ReviewFailedRun), g_RunId);
        Core_Fail(g_Generation, "selftest_forced_failure");
        Core_Dispatch(0, "nct_repeat", "");
        Selftests_ReviewCheck("failed_run_repeat", g_Resetting && g_RepeatAfterReset, "repeat accepts a failed run after active run_id is cleared");
        if (!g_Resetting) { Core_RoundReset(true, _:Scenario_HostageFollow, 1); }
        g_ReviewStep = 2;
    }
    else if (g_ReviewStep == 2 && !g_Resetting && g_CoreState == Core_Ready)
    {
        Selftests_ReviewCheck("failed_run_new_id", g_RunId[0] != 0 && !equal(g_RunId, g_ReviewFailedRun), "repeat prepares a different run_id");
        new anchor = Maps_FindAnchor("ct_route_end");
        new Float:coordinate = g_AnchorOrigins[anchor][0];
        new Float:origin[3], Float:after[3];
        pev(g_OperatorSlot, pev_origin, origin);
        g_AnchorOrigins[anchor][0] += 16.0;
        g_CoreState = Core_Running;
        Core_Dispatch(0, "nct_goto", "viewer_outside");
        pev(g_OperatorSlot, pev_origin, after);
        Selftests_ReviewCheck("active_profile_not_reloaded", g_AnchorOrigins[anchor][0] == coordinate + 16.0 && get_distance_f(origin, after) == 0.0,
            "goto cannot reload profile data or move the operator during an action");
        g_AnchorOrigins[anchor][0] = coordinate;
        g_CoreState = Core_Ready;
        Actors_EnsureTeam(g_OperatorSlot, TEAM_SPECTATOR);
        rg_set_observer_mode(g_OperatorSlot, OBS_ROAMING);
        set_member(g_OperatorSlot, m_hObserverTarget, 0);
        set_member(g_OperatorSlot, m_flNextFollowTime, 0.0);
        new userid[16];
        num_to_str(g_ActorUserids[1], userid, charsmax(userid));
        Core_Dispatch(0, "nct_observe", userid);
        g_ReviewNext = now + 0.5;
        g_ReviewStep = 3;
    }
    else if (g_ReviewStep == 3 && now >= g_ReviewNext)
    {
        Selftests_ReviewCheck("observer_named_target", pev(g_OperatorSlot, pev_iuser1) == OBS_IN_EYE && pev(g_OperatorSlot, pev_iuser2) == g_ActorSlots[1],
            "an observer without a target selects the requested CT instead of the first T");
        set_member(g_OperatorSlot, m_flNextFollowTime, now + 0.25);
        new userid[16];
        num_to_str(g_ActorUserids[0], userid, charsmax(userid));
        Core_Dispatch(0, "nct_observe", userid);
        g_ReviewNext = now + 0.5;
        g_ReviewStep = 4;
    }
    else if (g_ReviewStep == 4 && now >= g_ReviewNext)
    {
        Selftests_ReviewCheck("observer_cooldown_target", pev(g_OperatorSlot, pev_iuser1) == OBS_IN_EYE && pev(g_OperatorSlot, pev_iuser2) == g_ActorSlots[0],
            "a requested T target is selected after the existing GameDLL cooldown");
        Core_Dispatch(0, "nct_outcome", "rescue");
        Radar_ProbeCheck("selftest_finished", g_ReviewPassed, "review checks complete; next map must retain outcome rescue");
        g_ReviewStep = 0;
        Core_ChangeMap("cs_office", true, Scenario_HostageEnd, 1);
        Actors_Remove(true);
        g_HeadlessOperator = false;
    }
}

Selftests_ScenesTick(Float:now)
{
    if (!g_SceneTestStep) { return; }
    if (now > g_SelftestDeadline)
    {
        Core_Cancel("selftest_timeout");
        Actors_Remove(true);
        g_SceneTestStep = 0;
        g_HeadlessOperator = false;
        Radar_ProbeCheck("selftest_finished", false, "scene suite timed out");
        return;
    }
    if (g_SceneTestStep == 1 && Actors_IsJoined(3))
    {
        g_HeadlessOperator = true;
        g_OperatorSlot = g_ActorSlots[3];
        g_OperatorUserid = g_ActorUserids[3];
        g_SceneTestStep = 2;
    }
    else if (g_SceneTestStep == 2)
    {
        if (equal(g_SceneTestSuite, "hostage")) { copy(g_Outcome, charsmax(g_Outcome), g_SceneTestCase == 7 ? "rescue" : "kill"); }
        else if (equal(g_SceneTestSuite, "bomb"))
        {
            copy(g_Outcome, charsmax(g_Outcome), g_SceneTestCase < 2 ? "defuse" : g_SceneTestCase < 4 ? "explosion" : g_SceneTestCase < 6 ? "dropped" : "planted");
        }
        Core_RoundReset(true, _:Selftests_Scenario(), Selftests_Role());
        g_SceneTestStep = 3;
    }
    else if (g_SceneTestStep == 3 && !g_Resetting)
    {
        if (g_CoreState == Core_Ready) { Core_Dispatch(0, "nct_step", ""); }
        else if (g_CoreState == Core_Finished || g_CoreState == Core_Failed || g_CoreState == Core_Canceled)
        {
            new actual[128];
            formatex(actual, charsmax(actual), "%s case=%d repetition=%d state=%s", kScenarioNames[_:Selftests_Scenario()], g_SceneTestCase, g_SceneTestIteration + 1, kStateNames[_:g_CoreState]);
            new bool:passed = g_CoreState == Core_Finished;
            Radar_ProbeCheck("scene_repetition", passed, actual);
            Diagnostics_Report("selftest");
            if (!passed) { g_SceneTestFailures++; g_SceneTestIteration = g_SceneTestRepeats - 1; }
            if (++g_SceneTestIteration == g_SceneTestRepeats)
            {
                g_SceneTestIteration = 0;
                g_SceneTestCase++;
            }
            if (g_SceneTestCase == g_SceneTestEndCase)
            {
                new finished_run[64];
                copy(finished_run, charsmax(finished_run), g_RunId);
                Core_Cancel("selftest_complete");
                Radar_ProbeCheck("reset_run_context", !g_RunId[0] && equal(g_LastRunId, finished_run), "cleanup ends the run context and retains only last_run_id");
                Actors_Remove(true);
                g_HeadlessOperator = false;
                g_SceneTestStep = 0;
                g_CoreState = Core_Idle;
                formatex(actual, charsmax(actual), "%d repetitions per case; network PVS and visual remain not_run", g_SceneTestRepeats);
                Radar_ProbeCheck("selftest_finished", g_SceneTestFailures == 0, actual);
            }
            else { g_SceneTestStep = 2; }
        }
    }
}
