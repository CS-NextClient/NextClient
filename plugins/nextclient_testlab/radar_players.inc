new g_PlayerAlly;
new g_PlayerEnemy;
new Float:g_PlayerStart[3];
new Float:g_PlayerHealth;
new g_PlayerRadarBefore;

bool:Players_Prepare(ScenarioId:scenario, operator_id, generation)
{
    #pragma unused scenario, generation
    if (!Radar_RequireAnchor("viewer_open", Anchor_Player)) { return false; }
    if (!Actors_Create(0, TEAM_TERRORIST) || !Actors_Create(1, TEAM_CT) || !Actors_Create(2, TEAM_TERRORIST)) { return false; }
    new TeamName:team = g_OperatorRole == 1 ? TEAM_CT : g_OperatorRole == 2 ? TEAM_TERRORIST : TEAM_SPECTATOR;
    if (!Actors_EnsureTeam(operator_id, team)) { return false; }
    if (team != TEAM_SPECTATOR) { rg_round_respawn(operator_id); }
    g_PlayerAlly = g_OperatorRole == 2 ? 0 : 1;
    g_PlayerEnemy = g_PlayerAlly == 0 ? 1 : 0;
    g_RadarMoveMessages = 0;
    for (new actor = 0; actor < 3; actor++) { g_RadarActorMessages[actor] = 0; }
    g_BadMessageAudience = 0;
    return Core_SetCvar("mp_round_infinite", "abf");
}

bool:Players_StartStep(ScenarioId:scenario, step_index, generation)
{
    g_ActionStarted = get_gametime();
    g_CoreDeadline = g_ActionStarted + 15.0;
    if (scenario == Scenario_Lifecycle)
    {
        if (step_index == 0)
        {
            new TeamName:team = get_member(g_OperatorSlot, m_iTeam) == TEAM_CT ? TEAM_TERRORIST : TEAM_CT;
            if (!Actors_EnsureTeam(g_OperatorSlot, team)) { return false; }
            rg_round_respawn(g_OperatorSlot);
            Radar_ProbeCheck("lifecycle_role", get_member(g_OperatorSlot, m_iTeam) == team, "normal role change and respawn with the same operator userid");
            Core_StageFinished(generation);
        }
        else if (step_index == 1) { g_ProbeRestarted = false; rg_restart_round(); g_ScenePhase = 70; }
        else
        {
            new JSON:data = json_init_object();
            json_object_set_string(data, "id", step_index == 2 ? "lifecycle_map_client" : "lifecycle_reconnect_client");
            json_object_set_string(data, "scope", "visual");
            json_object_set_string(data, "status", "not_run");
            json_object_set_string(data, "expected", "old client markers and policy cleared; new operator before resume");
            json_object_set_string(data, "actual", "operator must perform nct_reset map / nct_map and disconnect / reconnect; record nct_result");
            Diagnostics_Emit("check", data);
            json_free(data);
            Core_Reply(g_OperatorSlot, step_index == 2 ? "record run_id; nct_reset map, then reconnect and start a new run" : "nct_map cs_office|de_dust2; reconnect, nct_resume within 120 seconds");
            Core_StageFinished(generation);
        }
        return true;
    }
    if (step_index == 0)
    {
        pev(g_ActorSlots[g_PlayerAlly], pev_origin, g_PlayerStart);
        g_PlayerRadarBefore = g_RadarActorMessages[g_PlayerAlly];
        if (!Actors_FindMove(g_PlayerAlly, 192.0)) { return false; }
        g_ScenePhase = 60;
    }
    else if (step_index == 1)
    {
        pev(g_ActorSlots[g_PlayerAlly], pev_health, g_PlayerHealth);
        ExecuteHamB(Ham_TakeDamage, g_ActorSlots[g_PlayerAlly], 0, 0, 35.0, DMG_GENERIC);
        g_ScenePhase = 61;
    }
    else if (step_index == 2)
    {
        ExecuteHamB(Ham_TakeDamage, g_ActorSlots[g_PlayerEnemy], 0, 0, 10000.0, DMG_GENERIC);
        g_ScenePhase = 62;
    }
    else
    {
        set_pev(g_OperatorSlot, pev_takedamage, DAMAGE_AIM);
        ExecuteHamB(Ham_TakeDamage, g_OperatorSlot, 0, 0, 10000.0, DMG_GENERIC);
        g_ScenePhase = 63;
    }
    return true;
}

Players_Tick(ScenarioId:scenario, generation, Float:now)
{
    #pragma unused now
    if (g_BadMessageAudience) { Core_Fail(generation, "player_radar_audience_failed"); return; }
    if (g_CoreState == Core_Preparing && Actors_AreJoined() && Actors_IsJoined(2))
    {
        if (g_OperatorRole == 4)
        {
            if (!(get_member(g_OperatorSlot, m_afPhysicsFlags) & PFLAG_OBSERVER)) { return; }
            rg_set_observer_mode(g_OperatorSlot, OBS_ROAMING);
        }
        if (!Radar_PlaceOperator("viewer_open")) { Core_Fail(generation, "setup_failed_player_viewer"); return; }
        Radar_ProbeCheck("players_joined", true, "three owned actors with distinct T/CT teams and live userids");
        Core_StageReady(generation);
        return;
    }
    if (g_CoreState != Core_Running) { return; }
    if (scenario == Scenario_Lifecycle)
    {
        if (g_ScenePhase == 70 && g_ProbeRestarted && Actors_AreJoined() && Actors_IsJoined(2))
        {
            new bool:valid = Actors_Count() == 3 && !Radar_FindBomb() && !Radar_FindBombBox();
            Radar_ProbeCheck("lifecycle_round", valid, "real restart hook, same owned actor count, no dropped/planted C4");
            if (!valid) { Core_Fail(generation, "lifecycle_round_postcondition_failed"); }
            else { Core_StageFinished(generation); }
        }
        return;
    }
    if (g_ScenePhase == 60)
    {
        new Float:origin[3];
        pev(g_ActorSlots[g_PlayerAlly], pev_origin, origin);
        if (get_distance_f(origin, g_PlayerStart) >= 128.0)
        {
            g_ActorForward[g_PlayerAlly] = 0.0;
            if (g_RadarActorMessages[g_PlayerAlly] > g_PlayerRadarBefore)
            {
                Radar_ProbeCheck("player_movement_radar", true, "normal input moved ally at least 128 units; same-team Radar observed");
                Core_StageFinished(generation);
            }
        }
    }
    else if (g_ScenePhase == 61)
    {
        new Float:health;
        pev(g_ActorSlots[g_PlayerAlly], pev_health, health);
        if (health > 0.0 && health < g_PlayerHealth)
        {
            Radar_ProbeCheck("player_health", true, "normal TakeDamage reduced health while retaining alive state");
            Core_StageFinished(generation);
        }
    }
    else if (g_ScenePhase == 62 && !is_user_alive(g_ActorSlots[g_PlayerEnemy]))
    {
        Radar_ProbeCheck("player_death", true, "normal damage/killed path removed enemy alive state");
        Core_StageFinished(generation);
    }
    else if (g_ScenePhase == 63 && !is_user_alive(g_OperatorSlot) && (get_member(g_OperatorSlot, m_afPhysicsFlags) & PFLAG_OBSERVER))
    {
        new result = Bomb_Observe(g_PlayerAlly);
        if (result < 0) { Core_Fail(generation, "setup_failed_player_observer_target"); return; }
        if (!result) { return; }
        Radar_ProbeCheck("player_observer", true, "normal operator death and IN_EYE target with verified ally userid");
        Core_StageFinished(generation);
    }
}
