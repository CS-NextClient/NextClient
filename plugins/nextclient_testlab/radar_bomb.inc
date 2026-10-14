new g_BombPicker;
new g_BombOperatorDrops;
new g_BombForbidden;
new g_BombCoordinateChecks;
new g_BombBadCoordinates;
new bool:g_BombPlantedView;
new bool:g_BombDropping;
new Float:g_BombDropOrigin[3];

Bomb_Carrier()
{
    for (new slot = 1; slot <= MaxClients; slot++)
    {
        if (is_user_connected(slot) && get_member(slot, m_bHasC4)) { return slot; }
    }
    return 0;
}

Bomb_Count()
{
    new count, entity;
    for (new slot = 1; slot <= MaxClients; slot++)
    {
        if (is_user_connected(slot) && get_member(slot, m_bHasC4)) { count++; }
    }
    while ((entity = Radar_FindClass("grenade", entity)))
    {
        if (get_member(entity, m_Grenade_bIsC4) && !(pev(entity, pev_flags) & FL_KILLME)) { count++; }
    }
    entity = 0;
    while ((entity = Radar_FindClass("weaponbox", entity)))
    {
        new model[64];
        pev(entity, pev_model, model, charsmax(model));
        if (equal(model, "models/w_backpack.mdl") && !(pev(entity, pev_flags) & FL_KILLME)) { count++; }
    }
    return count;
}

bool:Bomb_Give()
{
    new carrier = Bomb_Carrier(), terrorist = g_ActorSlots[0];
    if (carrier && carrier != terrorist)
    {
        new bool:owned = carrier == g_OperatorSlot;
        for (new actor = 0; actor < 3; actor++) { if (Actors_IsCurrent(actor) && carrier == g_ActorSlots[actor]) { owned = true; } }
        if (!owned || !rg_transfer_c4(carrier, terrorist)) { return false; }
    }
    else if (!carrier) { rg_give_item(terrorist, "weapon_c4"); }
    return get_member(terrorist, m_bHasC4) && Bomb_Count() == 1;
}

bool:Bomb_Prepare(ScenarioId:scenario, operator_id, generation)
{
    #pragma unused generation
    if (!Radar_RequireAnchor("bombsite_plant", Anchor_Player) || !Radar_RequireAnchor("ct_defuse", Anchor_Player)
        || !Radar_RequireAnchor("t_start", Anchor_Player) || !Radar_RequireAnchor("viewer_open", Anchor_Player)) { return false; }
    if (scenario == Scenario_BombVisibility && (Maps_FindView("open") < 0 || Maps_FindView("occluded") < 0 || Maps_FindView("outside") < 0)) { return false; }
    if (!Actors_Create(0, TEAM_TERRORIST) || !Actors_Create(1, TEAM_CT) || !Actors_Create(2, TEAM_TERRORIST)) { return false; }
    new TeamName:team = g_OperatorRole == 1 ? TEAM_CT : TEAM_TERRORIST;
    if (!Actors_EnsureTeam(operator_id, team)) { return false; }
    rg_round_respawn(operator_id);
    g_PlantedMessages = 0;
    g_DroppedMessages = 0;
    g_ClearMessages = 0;
    g_BadMessageAudience = 0;
    g_BombOperatorDrops = 0;
    g_BombForbidden = 0;
    g_BombCoordinateChecks = 0;
    g_BombBadCoordinates = 0;
    g_BombPicker = 0;
    g_BombPlantedView = bool:equal(g_Outcome, "planted");
    if (scenario == Scenario_BombVisibility) { g_CoreDeadline = get_gametime() + 30.0; }
    return Core_SetCvar("mp_c4timer", scenario == Scenario_BombVisibility ? "90" : equal(g_Outcome, "explosion") ? "10" : "35")
        && Core_SetCvar("mp_round_restart_delay", "120") && Core_SetCvar("mp_round_infinite", "abf");
}

bool:Bomb_Place(ScenarioId:scenario)
{
    if (!Radar_PlaceActor(0, "t_start") || !Radar_PlaceOperator("viewer_open")) { return false; }
    new Float:origin[3], Float:point[3], anchor = Maps_FindAnchor("bombsite_plant");
    if (scenario == Scenario_BombObserver)
    {
        if (!Radar_PlaceActor(1, "viewer_outside")) { return false; }
    }
    else
    {
        if (!Bomb_FindStaging(origin)) { return false; }
        Actors_Place(1, origin);
    }
    for (new axis = 0; axis < 3; axis++) { point[axis] = g_AnchorOrigins[anchor][axis]; }
    point[1] += 96.0;
    if (!Actors_FindFloor(point, origin)) { return false; }
    Actors_Place(2, origin);
    return true;
}

bool:Bomb_FindStaging(Float:origin[3])
{
    new anchor = Maps_FindAnchor("bombsite_plant"), Float:point[3];
    for (new radius = 96; radius <= 192; radius += 32)
    {
        for (new direction = 3; direction < 8; direction++)
        {
            point[0] = g_AnchorOrigins[anchor][0] + floatcos(float(direction * 45), degrees) * float(radius);
            point[1] = g_AnchorOrigins[anchor][1] + floatsin(float(direction * 45), degrees) * float(radius);
            point[2] = g_AnchorOrigins[anchor][2];
            if (floatabs(point[1] - g_AnchorOrigins[anchor][1]) < 64.0) { continue; }
            if (Actors_FindFloor(point, origin) && Geometry_ClearSegment(origin, g_AnchorOrigins[anchor])) { return true; }
        }
    }
    return false;
}

bool:Bomb_Drop()
{
    if (!Bomb_Give()) { return false; }
    pev(g_ActorSlots[0], pev_origin, g_BombDropOrigin);
    g_BombDropping = true;
    rg_drop_item(g_ActorSlots[0], "weapon_c4");
    g_BombDropping = false;
    g_ActorAngles[0][0] = 0.0;
    g_ActorAngles[0][1] = 0.0;
    g_ActorForward[0] = -120.0;
    g_ActionStarted = get_gametime();
    g_ScenePhase = 10;
    return true;
}

bool:Bomb_StartStep(ScenarioId:scenario, step_index, generation)
{
    #pragma unused generation
    g_ActionStarted = get_gametime();
    g_CoreDeadline = g_ActionStarted + 20.0;
    if (scenario == Scenario_BombVisibility)
    {
        new const views[][] = {"open", "occluded", "outside", "open"};
        new entity = g_BombPlantedView ? Radar_FindBomb() : Radar_FindBombBox();
        if (!Radar_StartView(views[step_index], entity)) { return false; }
        g_ScenePhase = 30;
        return true;
    }
    if (scenario == Scenario_BombObserver)
    {
        if (step_index == 0) { return Bomb_Drop(); }
        if (step_index == 1)
        {
            set_pev(g_OperatorSlot, pev_takedamage, DAMAGE_AIM);
            ExecuteHamB(Ham_TakeDamage, g_OperatorSlot, g_ActorSlots[1], g_ActorSlots[1], 10000.0, DMG_GENERIC);
            g_ScenePhase = 40;
        }
        else if (step_index == 2) { g_BombPicker = 2; g_ScenePhase = 12; }
        else { g_ScenePhase = 50; }
        return true;
    }
    if (step_index == 0)
    {
        if (!Bomb_Give()) { return false; }
        Radar_ProbeCheck("bomb_given", true, "one C4 in nct_t1 inventory; stock transfer/give path");
        Core_StageFinished(generation);
    }
    else if (step_index == 1) { return Bomb_Drop(); }
    else if (step_index == 2) { g_BombPicker = 0; g_ScenePhase = 12; }
    else if (step_index == 3) { rg_internal_cmd(g_ActorSlots[0], "weapon_c4"); g_ScenePhase = 13; }
    else
    {
        if (!equal(g_Outcome, "defuse") && !equal(g_Outcome, "explosion")) { return false; }
        g_ScenePhase = equal(g_Outcome, "defuse") ? 15 : 16;
        g_CoreDeadline = g_ActionStarted + 120.0;
    }
    return true;
}

Bomb_Observe(actor)
{
    new target = g_ActorSlots[actor];
    if (!Actors_IsCurrent(actor)) { return -1; }
    return Actors_TryObserve(g_OperatorSlot, target, g_ActorUserids[actor]);
}

Bomb_Tick(ScenarioId:scenario, generation, Float:now)
{
    if (g_BadMessageAudience || g_BombForbidden || g_BombBadCoordinates || Bomb_Count() > 1)
    {
        new actual[128];
        formatex(actual, charsmax(actual), "count=%d audience=%d forbidden=%d coordinates=%d/%d phase=%d", Bomb_Count(), g_BadMessageAudience, g_BombForbidden, g_BombBadCoordinates, g_BombCoordinateChecks, g_ScenePhase);
        Radar_ProbeCheck("bomb_observed_state", false, actual);
        Core_Fail(generation, "bomb_count_audience_or_coordinates_failed");
        return;
    }
    if (g_CoreState == Core_Preparing && g_ScenePhase == 0 && Actors_AreJoined() && Actors_IsJoined(2))
    {
        if (!Bomb_Place(scenario)) { Core_Fail(generation, "setup_failed_bomb_placement"); return; }
        if (scenario == Scenario_BombVisibility)
        {
            g_ScenePhase = 1;
            g_ActionStarted = now;
        }
        else { Core_StageReady(generation); }
        return;
    }
    if (g_CoreState == Core_Preparing && g_ScenePhase == 1)
    {
        new Float:angles[3];
        pev(g_ActorSlots[0], pev_v_angle, angles);
        if (now - g_ActionStarted >= 0.25 && floatabs(angles[1] - g_ActorAngles[0][1]) < 1.0
            && Float:get_member(g_ActorSlots[0], m_flNextAttack) <= 0.0)
        {
            if (!Bomb_Drop()) { Core_Fail(generation, "setup_failed_bomb_give_drop"); }
        }
        return;
    }
    if (g_CoreState != Core_Running && g_CoreState != Core_Preparing) { return; }
    if (g_ScenePhase == 10 && now - g_ActionStarted > 0.5 && Radar_FindBombBox() && !get_member(g_ActorSlots[0], m_bHasC4))
    {
        Actors_ReleaseInput();
        new bool:valid = Bomb_Count() == 1 && g_DroppedMessages > 0 && g_BombCoordinateChecks > 0
            && (scenario != Scenario_BombObserver || g_BombOperatorDrops > 0);
        Radar_ProbeCheck("bomb_dropped", valid, "one real weaponbox, alive-T recipients and contemporaneous WRITE_COORD positions");
        if (!valid) { Core_Fail(generation, "bomb_drop_postcondition_failed"); return; }
        if (scenario == Scenario_BombVisibility && g_BombPlantedView) { g_ScenePhase = 12; }
        else if (g_CoreState == Core_Preparing) { Core_StageReady(generation); }
        else { Core_StageFinished(generation); }
    }
    else if (g_ScenePhase == 12)
    {
        new box = Radar_FindBombBox();
        if (box)
        {
            new Float:origin[3];
            pev(box, pev_origin, origin);
            Radar_MoveToward(g_BombPicker, origin);
        }
        else if (get_member(g_ActorSlots[g_BombPicker], m_bHasC4) && Bomb_Count() == 1)
        {
            Actors_ReleaseInput();
            Radar_ProbeCheck("bomb_picked_up", true, "Touch removed weaponbox and restored the intended T inventory");
            if (g_CoreState == Core_Preparing) { rg_internal_cmd(g_ActorSlots[0], "weapon_c4"); g_ScenePhase = 13; }
            else if (scenario == Scenario_BombObserver) { g_ScenePhase = 41; g_ActionStarted = now; }
            else { Core_StageFinished(generation); }
        }
    }
    else if (g_ScenePhase == 13)
    {
        new anchor = Maps_FindAnchor("bombsite_plant"), Float:origin[3];
        pev(g_ActorSlots[0], pev_origin, origin);
        if (get_distance_f(origin, g_AnchorOrigins[anchor]) > 16.0) { Radar_MoveToward(0, g_AnchorOrigins[anchor]); return; }
        g_ActorForward[0] = 0.0;
        new signals[UnifiedSignals];
        get_member(g_ActorSlots[0], m_signals, signals);
        if ((signals[US_State] & _:SIGNAL_BOMB) && (pev(g_ActorSlots[0], pev_flags) & FL_ONGROUND)
            && get_user_weapon(g_ActorSlots[0]) == CSW_C4 && Float:get_member(g_ActorSlots[0], m_flNextAttack) <= 0.0)
        {
            g_ActorButtons[0] = IN_ATTACK;
            g_ScenePhase = 14;
        }
    }
    else if (g_ScenePhase == 14 && Radar_FindBomb())
    {
        Actors_ReleaseInput();
        new bool:valid = Bomb_Count() == 1 && !Bomb_Carrier() && g_PlantedMessages > 0;
        Radar_ProbeCheck("bomb_planted", valid, "normal IN_ATTACK created one C4 grenade and planted BombDrop MSG_ALL");
        if (!valid) { Core_Fail(generation, "bomb_plant_postcondition_failed"); return; }
        if (!Radar_FindRetreat(0, Radar_FindBomb(), "viewer_open")) { Core_Fail(generation, "setup_failed_planter_retreat"); return; }
        g_ScenePhase = 17;
    }
    else if (g_ScenePhase == 15)
    {
        new bomb = Radar_FindBomb();
        if (!bomb) { Core_Fail(generation, "setup_failed_bomb_missing_before_defuse"); return; }
        new Float:origin[3], Float:target[3];
        pev(g_ActorSlots[1], pev_origin, origin);
        pev(bomb, pev_origin, target);
        if (get_distance_f(origin, target) > 60.0) { Radar_MoveToward(1, target); return; }
        g_ActorForward[1] = 0.0;
        Radar_AimUse(1, bomb);
        g_ActorButtons[1] = IN_USE;
        g_UseFrames = 0;
        g_ScenePhase = 16;
    }
    else if (g_ScenePhase == 17)
    {
        new Float:origin[3];
        pev(g_ActorSlots[0], pev_origin, origin);
        if (get_distance_f(origin, g_SceneTarget) > 16.0) { Radar_MoveToward(0, g_SceneTarget); }
        else
        {
            Actors_ReleaseInput();
            if (g_CoreState == Core_Preparing) { Core_StageReady(generation); }
            else { Core_StageFinished(generation); }
        }
    }
    else if (g_ScenePhase == 16 && !Radar_FindBomb())
    {
        new bool:valid = (equal(g_Outcome, "defuse") ? get_member_game(m_bBombDefused) : get_member_game(m_bTargetBombed))
            && !Bomb_Count() && g_ClearMessages > 0;
        Radar_ProbeCheck("bomb_outcome", valid, "GameDLL outcome, no C4 and normal BombPickup MSG_ALL clearing");
        if (!valid) { Core_Fail(generation, "bomb_outcome_postcondition_failed"); }
        else { Core_StageFinished(generation); }
    }
    else if (g_ScenePhase == 30)
    {
        if (!(g_BombPlantedView ? Radar_FindBomb() : Radar_FindBombBox())) { Core_Fail(generation, "bomb_visibility_state_changed"); return; }
        new result = Diagnostics_ViewResult();
        if (result < 0) { Core_Fail(generation, "setup_failed_bomb_view_pvs; calibrate viewer anchor"); }
        else if (result > 0 && now - g_ActionStarted >= 2.0) { Core_StageFinished(generation); }
    }
    else if (g_ScenePhase == 40 && !is_user_alive(g_OperatorSlot) && (get_member(g_OperatorSlot, m_afPhysicsFlags) & PFLAG_OBSERVER))
    {
        Radar_ProbeCheck("bomb_operator_death", get_member(g_OperatorSlot, m_iTeam) == TEAM_TERRORIST, "damage/killed path retains T team and enters observer");
        Core_StageFinished(generation);
    }
    else if (g_ScenePhase == 41 && now - g_ActionStarted >= 2.0)
    {
        Radar_ProbeCheck("dead_t_no_new_report", !g_BombForbidden, "complete two-second window: no new unicast BombDrop/BombPickup to dead operator");
        Core_StageFinished(generation);
    }
    else if (g_ScenePhase == 50)
    {
        new result = Bomb_Observe(2);
        if (result < 0) { Core_Fail(generation, "setup_failed_t_observer_target"); return; }
        if (!result) { return; }
        Radar_ProbeCheck("observer_t_target", true, "IN_EYE target slot and owned T userid agree");
        g_ActionStarted = now;
        g_ScenePhase = 51;
    }
    else if (g_ScenePhase == 51 && now - g_ActionStarted >= 2.0)
    {
        new result = Bomb_Observe(1);
        if (result < 0) { Core_Fail(generation, "setup_failed_ct_observer_target"); return; }
        if (!result) { return; }
        Radar_ProbeCheck("observer_ct_target", true, "IN_EYE target slot and owned CT userid agree; local team retained");
        Core_StageFinished(generation);
    }
}

Bomb_ObserveMessage(const name[], destination, recipient)
{
    if (g_Scenario < Scenario_BombCycle || g_Scenario > Scenario_BombObserver
        || (g_CoreState != Core_Preparing && g_CoreState != Core_Running)) { return; }
    if (!g_BombDropping && g_CoreState == Core_Preparing && g_ScenePhase < 10) { return; }
    if (recipient == g_OperatorSlot && !is_user_alive(recipient) && destination != MSG_ALL) { g_BombForbidden++; }
    if (equal(name, "BombDrop"))
    {
        if (!get_msg_arg_int(4) && recipient == g_OperatorSlot) { g_BombOperatorDrops++; }
        new entity = get_msg_arg_int(4) ? Radar_FindBomb() : Radar_FindBombBox();
        if (entity || g_BombDropping)
        {
            new Float:origin[3];
            if (g_BombDropping) { for (new axis = 0; axis < 3; axis++) { origin[axis] = g_BombDropOrigin[axis]; } }
            else { pev(entity, pev_origin, origin); }
            g_BombCoordinateChecks++;
            for (new axis = 0; axis < 3; axis++)
            {
                if (floatabs(origin[axis] - get_msg_arg_float(axis + 1)) > 0.126) { g_BombBadCoordinates++; }
            }
        }
    }
}
