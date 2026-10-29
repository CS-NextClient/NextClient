enum ProbeKind
{
    Probe_None,
    Probe_Actors,
    Probe_Bomb,
    Probe_Hostage
};

new ProbeKind:g_Probe;
new g_ProbeStep;
new g_ProbeIteration;
new Float:g_ProbeDeadline;
new Float:g_ProbeStarted;
new Float:g_ProbeOrigin[3];
new Float:g_BombsiteOrigin[3];
new g_ProbeHostage;
new g_HostageIds[64];
new g_HostageEntities[64];
new g_HostageCount;
new g_ProbeRestartCount;
new bool:g_ProbeRestarted;
new g_PlantedMessages;
new g_ClearMessages;
new g_DroppedMessages;
new bool:g_TraceBeforeProbe;
new g_HostageInitialMessages;
new g_HostageMoveMessages;
new g_HostageEndMessages;
new g_RadarMoveMessages;
new g_RadarActorMessages[3];
new g_BadMessageAudience;
new g_ScenePhase;
new g_SceneGeneration;
new g_SceneHostage;
new g_SceneHostageId;
new Float:g_ActionStarted;
new Float:g_SceneTarget[3];
new g_SceneRoute = -1;
new g_SceneWaypoint;
new Float:g_WaypointDeadline;
new bool:g_SceneRescue;
new g_SceneRescuedBefore;
new g_SceneMessageBefore;
new g_WireHostageIds[2048];
new bool:g_HostageIdAdapter;
new bool:g_ProbeBaselineSaved;
new g_UseFrames;

Radar_HostageId(entity)
{
    new native_id = cs_get_hostage_id(entity);
    if (entity >= sizeof g_WireHostageIds) { return 0; }
    new wire_id = g_WireHostageIds[entity];
    if (wire_id > 0 && native_id != wire_id) { g_HostageIdAdapter = true; }
    return wire_id > 0 ? wire_id : native_id;
}

Radar_ObserveHostageId()
{
    new Float:sent[3];
    for (new axis = 0; axis < 3; axis++) { sent[axis] = get_msg_arg_float(axis + 3); }
    new entity, matched, count;
    while ((entity = Radar_FindClass("hostage_entity", entity)))
    {
        new Float:origin[3];
        pev(entity, pev_origin, origin);
        if (get_distance_f(origin, sent) <= 0.25) { matched = entity; count++; }
    }
    new wire_id = get_msg_arg_int(2);
    if (count == 1 && matched < sizeof g_WireHostageIds && wire_id > 0 && wire_id <= 255)
    {
        g_WireHostageIds[matched] = wire_id;
        Radar_HostageId(matched);
    }
}

bool:Radar_UsesHostageIdAdapter()
{
    return g_HostageIdAdapter;
}

Radar_FindClass(const classname[], start = 0)
{
    return engfunc(EngFunc_FindEntityByString, start, "classname", classname);
}

Radar_FindBomb()
{
    new entity;
    while ((entity = Radar_FindClass("grenade", entity)))
    {
        if (get_member(entity, m_Grenade_bIsC4) && !(pev(entity, pev_flags) & FL_KILLME))
        {
            return entity;
        }
    }
    return 0;
}

Radar_FindBombBox()
{
    new entity;
    while ((entity = Radar_FindClass("weaponbox", entity)))
    {
        new model[64];
        pev(entity, pev_model, model, charsmax(model));
        if (equal(model, "models/w_backpack.mdl") && !(pev(entity, pev_flags) & FL_KILLME))
        {
            return entity;
        }
    }
    return 0;
}

bool:Radar_FindBombsite(Float:origin[3])
{
    new entity;
    while ((entity = Radar_FindClass("func_bomb_target", entity)))
    {
        new Float:minimum[3], Float:maximum[3], Float:point[3];
        pev(entity, pev_absmin, minimum);
        pev(entity, pev_absmax, maximum);
        for (new axis = 0; axis < 3; axis++)
        {
            point[axis] = (minimum[axis] + maximum[axis]) * 0.5;
        }
        if (Actors_FindFloor(point, origin))
        {
            return true;
        }
    }
    return false;
}

bool:Radar_SaveHostages(bool:compare, bool:require_ids = true)
{
    new entity, count;
    while ((entity = Radar_FindClass("hostage_entity", entity)))
    {
        if (count >= sizeof g_HostageIds)
        {
            return false;
        }
        new hostage_id = Radar_HostageId(entity);
        if (require_ids && hostage_id <= 0) { return false; }
        for (new index = 0; index < count; index++)
        {
            if (hostage_id > 0 && g_HostageIds[index] == hostage_id) { return false; }
        }
        if (compare && (g_HostageEntities[count] != entity || (g_HostageIds[count] > 0 && g_HostageIds[count] != hostage_id)))
        {
            return false;
        }
        g_HostageEntities[count] = entity;
        g_HostageIds[count++] = hostage_id;
    }
    if (compare && count != g_HostageCount)
    {
        return false;
    }
    g_HostageCount = count;
    return count > 0;
}

Radar_CurrentObjective()
{
    if (g_Scenario <= Scenario_HostageEnd && pev_valid(g_SceneHostage)) { return g_SceneHostage; }
    new bomb = Radar_FindBomb(), box = Radar_FindBombBox();
    return bomb ? bomb : box ? box : Radar_FindClass("hostage_entity");
}

Radar_ProbeCheck(const check_id[], bool:passed, const actual[])
{
    new JSON:data = json_init_object();
    json_object_set_string(data, "id", check_id);
    json_object_set_string(data, "scope", "server");
    json_object_set_string(data, "status", passed ? "pass" : "fail");
    json_object_set_string(data, "expected", "observed game postcondition");
    json_object_set_string(data, "actual", actual);
    Diagnostics_Emit("check", data);
    json_free(data);
}

Radar_ProbeFail(const code[])
{
    Radar_ProbeCheck("probe_finished", false, code);
    Actors_Remove();
    g_Probe = Probe_None;
    Diagnostics_SetTrace(g_TraceBeforeProbe);
}

Radar_ProbeNext(step, Float:timeout)
{
    g_ProbeStep = step;
    g_ProbeStarted = get_gametime();
    g_ProbeDeadline = g_ProbeStarted + timeout;
}

public Radar_ProbeCommand()
{
    if (g_Probe != Probe_None || Core_IsCallbackCurrent(g_Generation) || g_Resetting || Actors_Count(true))
    {
        server_print("TestLab: busy");
        return PLUGIN_HANDLED;
    }
    if (!Maps_CheckProfile())
    {
        Radar_ProbeCheck("probe_finished", false, "setup_failed_map_profile_or_bsp_hash");
        return PLUGIN_HANDLED;
    }
    new argument[16];
    read_argv(1, argument, charsmax(argument));
    if (equal(argument, "actors"))
    {
        g_Probe = Probe_Actors;
    }
    else if (equal(argument, "bomb"))
    {
        g_Probe = Probe_Bomb;
    }
    else if (equal(argument, "hostage"))
    {
        g_Probe = Probe_Hostage;
    }
    else
    {
        server_print("Usage: nct_probe actors|bomb|hostage");
        return PLUGIN_HANDLED;
    }
    g_ProbeIteration = 0;
    g_ProbeBaselineSaved = false;
    g_ProbeRestartCount = 0;
    g_DroppedMessages = 0;
    g_PlantedMessages = 0;
    g_ClearMessages = 0;
    g_TraceBeforeProbe = g_TraceEnabled;
    Diagnostics_SetTrace(true);
    Diagnostics_ResetTrace();
    rg_restart_round();
    Radar_ProbeNext(0, 5.0);
    return PLUGIN_HANDLED;
}

public Radar_RoundRestarted()
{
    g_ProbeRestarted = true;
    g_ResetFinished = true;
}

public Radar_HostageUsed(entity, activator, caller, use_type, Float:value)
{
    if (g_Probe == Probe_Hostage || (g_CoreState == Core_Running && g_Scenario <= Scenario_HostageEnd))
    {
        new actual[96];
        formatex(actual, charsmax(actual), "entity=%d activator=%d follower=%d type=%d", entity, activator, cs_get_hostage_foll(entity), use_type);
        Radar_ProbeCheck("hostage_use_called", true, actual);
    }
}

public Radar_InputObserved(id)
{
    if (g_CoreState == Core_Running && id == g_ActorSlots[1] && (get_member(id, m_afButtonPressed) & IN_USE))
    {
        new actual[128], Float:next_attack;
        next_attack = Float:get_member(id, m_flNextAttack);
        formatex(actual, charsmax(actual), "uid=%d buttons=%d pressed=%d next_attack=%.3f", get_user_userid(id), pev(id, pev_button), get_member(id, m_afButtonPressed), next_attack);
        Radar_ProbeCheck("use_input_observed", true, actual);
    }
}

Radar_InputFrame(actor, msec)
{
    if (g_CoreState != Core_Running || actor != 1 || !(g_ActorButtons[actor] & IN_USE) || g_UseFrames++) { return; }
    new actual[160], id = g_ActorSlots[actor];
    formatex(actual, charsmax(actual), "msec=%d desired=%d actual=%d pressed=%d flags=%d phase=%d", msec, g_ActorButtons[actor], pev(id, pev_button), get_member(id, m_afButtonPressed), pev(id, pev_flags), g_ScenePhase);
    Radar_ProbeCheck("use_frame_observed", true, actual);
}

public Radar_StartFrame()
{
    new Float:now = get_gametime();
    if (!Actors_Tick(now))
    {
        if (g_Probe != Probe_None)
        {
            Radar_ProbeFail("actor_tick_gap");
        }
        else if (g_CoreState == Core_Preparing || g_CoreState == Core_Running || g_CoreState == Core_Ready)
        {
            Core_Fail(g_Generation, "setup_failed_actor_tick_gap");
        }
    }
    Selftests_Tick(now);
    Geometry_Tick(now);
    Core_Tick(now);
    if (g_Probe == Probe_None)
    {
        return FMRES_IGNORED;
    }
    if (now > g_ProbeDeadline)
    {
        for (new actor = 0; actor < sizeof g_ActorSlots; actor++)
        {
            if (Actors_IsCurrent(actor))
            {
                new detail[256];
                new Float:origin[3], Float:view[3];
                pev(g_ActorSlots[actor], pev_origin, origin);
                pev(g_ActorSlots[actor], pev_v_angle, view);
                new Float:maxspeed;
                pev(g_ActorSlots[actor], pev_maxspeed, maxspeed);
                formatex(detail, charsmax(detail), "slot=%d uid=%d join=%d alive=%d pos=%.2f %.2f %.2f speed=%.1f buttons=%d fixangle=%d view=%.1f %.1f desired=%.1f %.1f freeze=%d", g_ActorSlots[actor], g_ActorUserids[actor], get_member(g_ActorSlots[actor], m_iJoiningState), is_user_alive(g_ActorSlots[actor]), origin[0], origin[1], origin[2], maxspeed, pev(g_ActorSlots[actor], pev_button), pev(g_ActorSlots[actor], pev_fixangle), view[0], view[1], g_ActorAngles[actor][0], g_ActorAngles[actor][1], get_member_game(m_bFreezePeriod));
                Radar_ProbeCheck("actor_timeout_state", false, detail);
            }
        }
        new actual[128];
        formatex(actual, charsmax(actual), "timeout probe=%d step=%d actors=%d bomb=%d box=%d", _:g_Probe, g_ProbeStep, Actors_Count(), Radar_FindBomb(), Radar_FindBombBox());
        Radar_ProbeFail(actual);
        return FMRES_IGNORED;
    }
    Radar_ProbeTick(now);
    return FMRES_IGNORED;
}

bool:Radar_Prepare(ScenarioId:scenario, operator_id, generation)
{
    g_SceneGeneration = generation;
    g_ScenePhase = 0;
    if (scenario == Scenario_HostageFollow) { Hostage_FollowReset(); }
    if (!Maps_CheckProfile())
    {
        Core_Fail(generation, "setup_failed_map_profile_or_bsp_hash");
        return false;
    }
    if (scenario <= Scenario_HostageEnd)
    {
        return Hostage_Prepare(scenario, operator_id, generation);
    }
    if (scenario <= Scenario_BombObserver) { return Bomb_Prepare(scenario, operator_id, generation); }
    return Players_Prepare(scenario, operator_id, generation);
}

bool:Radar_StartStep(ScenarioId:scenario, step_index, generation)
{
    if (generation != g_SceneGeneration) { return false; }
    if (scenario <= Scenario_HostageEnd) { return Hostage_StartStep(scenario, step_index, generation); }
    if (scenario <= Scenario_BombObserver) { return Bomb_StartStep(scenario, step_index, generation); }
    return Players_StartStep(scenario, step_index, generation);
}

Radar_Tick(ScenarioId:scenario, generation, Float:now)
{
    if (generation != g_SceneGeneration) { return; }
    if (scenario <= Scenario_HostageEnd) { Hostage_Tick(scenario, generation, now); }
    else if (scenario <= Scenario_BombObserver) { Bomb_Tick(scenario, generation, now); }
    else { Players_Tick(scenario, generation, now); }
}

Radar_Stop(ScenarioId:scenario, generation)
{
    #pragma unused scenario, generation
    Actors_ReleaseInput();
    g_ScenePhase = -1;
    g_SceneRoute = -1;
    Diagnostics_StopView();
}

bool:Radar_RequireAnchor(const name[], AnchorKind:kind)
{
    new anchor = Maps_FindAnchor(name);
    if (anchor < 0 || g_AnchorKinds[anchor] != kind || (kind == Anchor_Player && !Maps_CheckPlayerAnchor(anchor)))
    {
        new code[128];
        formatex(code, charsmax(code), "setup_failed_anchor_%s; calibrate: nct_anchor %s", name, name);
        Core_Fail(g_SceneGeneration, code);
        return false;
    }
    return true;
}

bool:Radar_PlaceActor(actor, const name[])
{
    new anchor = Maps_FindAnchor(name);
    if (!Maps_CheckPlayerAnchor(anchor) || !Actors_IsCurrent(actor)) { return false; }
    Actors_Place(actor, g_AnchorOrigins[anchor]);
    for (new axis = 0; axis < 3; axis++) { g_ActorAngles[actor][axis] = g_AnchorAngles[anchor][axis]; }
    return true;
}

bool:Radar_PlaceOperator(const name[])
{
    if (g_HeadlessOperator) { return Radar_PlaceActor(3, name); }
    return Maps_PlacePlayer(g_OperatorSlot, name);
}

Radar_AimUse(actor, entity)
{
    new Float:minimum[3], Float:size[3], Float:target[3];
    pev(entity, pev_absmin, minimum);
    pev(entity, pev_size, size);
    for (new axis = 0; axis < 3; axis++) { target[axis] = minimum[axis] + size[axis] * 0.5; }
    Actors_AimAt(actor, target);
}

bool:Radar_StartView(const name[], entity)
{
    new view = Maps_FindView(name);
    if (view < 0 || !Radar_PlaceOperator(g_AnchorNames[g_ViewAnchors[view][0]])) { return false; }
    return Diagnostics_StartView(name, entity);
}

bool:Radar_StartRoute(const name[])
{
    g_SceneRoute = Maps_FindRoute(name);
    g_SceneWaypoint = 0;
    if (g_SceneRoute < 0) { return false; }
    g_WaypointDeadline = get_gametime() + g_RouteTimeouts[g_SceneRoute];
    return true;
}

Radar_RouteTick(actor, Float:now)
{
    if (g_SceneRoute < 0 || !Actors_IsCurrent(actor)) { return -1; }
    if (now > g_WaypointDeadline) { return -1; }
    new anchor = g_RouteAnchors[g_SceneRoute][g_SceneWaypoint];
    new Float:origin[3];
    pev(g_ActorSlots[actor], pev_origin, origin);
    if (get_distance_f(origin, g_AnchorOrigins[anchor]) <= g_RouteRadii[g_SceneRoute])
    {
        if (++g_SceneWaypoint == g_RouteLengths[g_SceneRoute])
        {
            Actors_ReleaseInput();
            return 1;
        }
        anchor = g_RouteAnchors[g_SceneRoute][g_SceneWaypoint];
        g_WaypointDeadline = now + g_RouteTimeouts[g_SceneRoute];
        if (!Geometry_ClearSegment(origin, g_AnchorOrigins[anchor])) { return -1; }
    }
    Actors_AimAt(actor, g_AnchorOrigins[anchor]);
    g_ActorAngles[actor][0] = 0.0;
    g_ActorForward[actor] = floatmin(g_RouteSpeeds[g_SceneRoute], floatmax(80.0, get_distance_f(origin, g_AnchorOrigins[anchor]) * 4.0));
    return 0;
}

Radar_MoveToward(actor, const Float:target[3], Float:speed = 120.0)
{
    Actors_AimAt(actor, target);
    g_ActorAngles[actor][0] = 0.0;
    g_ActorForward[actor] = speed;
}

JSON:Radar_ObjectiveJson(entity, const classname[], const bomb_state[] = "")
{
    new JSON:objective = json_init_object();
    json_object_set_number(objective, "entity_index", entity);
    json_object_set_string(objective, "classname", classname);
    json_object_set_null(objective, "owner_userid");
    if (equal(classname, "hostage_entity"))
    {
        json_object_set_number(objective, "hostage_id", Radar_HostageId(entity));
        json_object_set_number(objective, "native_hostage_id", cs_get_hostage_id(entity));
        json_object_set_null(objective, "bomb_state");
        new follower = cs_get_hostage_foll(entity);
        if (follower > 0 && follower <= MaxClients && is_user_connected(follower))
        {
            json_object_set_number(objective, "owner_userid", get_user_userid(follower));
        }
    }
    else
    {
        json_object_set_null(objective, "hostage_id");
        json_object_set_string(objective, "bomb_state", bomb_state);
        new owner = pev(entity, pev_owner);
        if (owner > 0 && owner <= MaxClients && is_user_connected(owner)) { json_object_set_number(objective, "owner_userid", get_user_userid(owner)); }
    }
    new Float:origin[3], Float:health;
    pev(entity, pev_origin, origin);
    pev(entity, pev_health, health);
    json_object_set_real(objective, "health", health);
    json_object_set_number(objective, "deadflag", pev(entity, pev_deadflag));
    new JSON:position = json_init_array();
    for (new axis = 0; axis < 3; axis++) { json_array_append_real(position, origin[axis]); }
    json_object_set_value(objective, "origin", position);
    json_free(position);
    return objective;
}

JSON:Radar_ObjectivesJson()
{
    new JSON:objectives = json_init_array();
    new entity;
    while ((entity = Radar_FindClass("hostage_entity", entity)))
    {
        new JSON:objective = Radar_ObjectiveJson(entity, "hostage_entity");
        json_array_append_value(objectives, objective);
        json_free(objective);
    }
    new bomb = Radar_FindBomb(), box = Radar_FindBombBox();
    new carrier = Bomb_Carrier();
    if (carrier)
    {
        new weapon = get_member(carrier, m_rgpPlayerItems, C4_SLOT);
        if (pev_valid(weapon))
        {
            new JSON:objective = Radar_ObjectiveJson(weapon, "weapon_c4", "carried");
            json_object_set_number(objective, "owner_userid", get_user_userid(carrier));
            new Float:origin[3], JSON:position = json_init_array();
            pev(carrier, pev_origin, origin);
            for (new axis = 0; axis < 3; axis++) { json_array_append_real(position, origin[axis]); }
            json_object_set_value(objective, "origin", position);
            json_free(position);
            json_array_append_value(objectives, objective);
            json_free(objective);
        }
    }
    if (bomb || box)
    {
        new JSON:objective = Radar_ObjectiveJson(bomb ? bomb : box, bomb ? "grenade" : "weaponbox", bomb ? "planted" : "dropped");
        json_array_append_value(objectives, objective);
        json_free(objective);
    }
    return objectives;
}

Radar_ProbeTick(Float:now)
{
    if (g_ProbeStep == 0)
    {
        if (!Actors_Create(0, TEAM_TERRORIST) || !Actors_Create(1, TEAM_CT))
        {
            Radar_ProbeFail("setup_failed_actor_connect_or_menu");
            return;
        }
        Radar_ProbeNext(1, 5.0);
    }
    else if (g_ProbeStep == 1 && Actors_AreJoined())
    {
        if (g_Probe == Probe_Hostage && !g_ProbeBaselineSaved)
        {
            if (!Radar_SaveHostages(false)) { return; }
            g_ProbeBaselineSaved = true;
        }
        Radar_ProbeCheck("actors_joined", true, "T and TEAM_CT connected, JOINED and alive");
        if (g_Probe == Probe_Actors)
        {
            pev(g_ActorSlots[0], pev_origin, g_ProbeOrigin);
            Radar_ProbeNext(2, 3.0);
        }
        else if (g_Probe == Probe_Bomb)
        {
            if (!Radar_FindBombsite(g_BombsiteOrigin))
            {
                Radar_ProbeFail("setup_failed_calibrate_bombsite_plant_with_nct_anchor");
                return;
            }
            Actors_Place(0, g_BombsiteOrigin);
            if (!get_member(g_ActorSlots[0], m_bHasC4))
            {
                rg_give_item(g_ActorSlots[0], "weapon_c4");
            }
            Radar_ProbeNext(10, 3.0);
        }
        else
        {
            g_ProbeHostage = Radar_FindClass("hostage_entity");
            new Float:hostage_origin[3], Float:actor_origin[3];
            pev(g_ProbeHostage, pev_origin, hostage_origin);
            if (!Actors_FindNear(hostage_origin, actor_origin, 48.0))
            {
                Radar_ProbeFail("setup_failed_calibrate_ct_use_with_nct_anchor");
                return;
            }
            Actors_Place(1, actor_origin);
            new Float:minimum[3], Float:size[3];
            pev(g_ProbeHostage, pev_absmin, minimum);
            pev(g_ProbeHostage, pev_size, size);
            for (new axis = 0; axis < 3; axis++)
            {
                hostage_origin[axis] = minimum[axis] + size[axis] * 0.5;
            }
            Actors_AimAt(1, hostage_origin);
            Radar_ProbeNext(19, 5.0);
        }
    }
    else if (g_Probe == Probe_Actors)
    {
        Radar_ProbeActors(now);
    }
    else if (g_Probe == Probe_Bomb)
    {
        Radar_ProbeBomb(now);
    }
    else if (g_Probe == Probe_Hostage)
    {
        Radar_ProbeHostage(now);
    }
}

Radar_ProbeActors(Float:now)
{
    if (g_ProbeStep == 2 && now - g_ProbeStarted > 0.2)
    {
        if (!Actors_FindMove(0))
        {
            Radar_ProbeFail("setup_failed_no_free_movement_segment");
            return;
        }
        Radar_ProbeNext(3, 3.0);
    }
    else if (g_ProbeStep == 3)
    {
        new Float:origin[3];
        pev(g_ActorSlots[0], pev_origin, origin);
        if (get_distance_f(origin, g_ProbeOrigin) >= 16.0)
        {
            Radar_ProbeCheck("actor_movement", true, "RunPlayerMove displaced T by at least 16 units");
            Actors_Remove();
            Radar_ProbeNext(4, 2.0);
        }
    }
    else if (g_ProbeStep == 4 && now - g_ProbeStarted > 0.1)
    {
        new name[32];
        for (new slot = 1; slot <= MaxClients; slot++)
        {
            if (is_user_connected(slot))
            {
                get_user_name(slot, name, charsmax(name));
                if (equal(name, kActorNames[0]) || equal(name, kActorNames[1]))
                {
                    Radar_ProbeFail("actor_kick_left_owned_slot");
                    return;
                }
            }
        }
        if (++g_ProbeIteration < 5)
        {
            Radar_ProbeNext(0, 5.0);
        }
        else
        {
            Radar_ProbeCheck("probe_finished", true, "five create/input/kick cycles; zero remaining actors");
            g_Probe = Probe_None;
            Diagnostics_SetTrace(g_TraceBeforeProbe);
        }
    }
}

Radar_ProbeBomb(Float:now)
{
    new terrorist = g_ActorSlots[0], counter_terrorist = g_ActorSlots[1];
    if (g_ProbeStep == 10 && get_member(terrorist, m_bHasC4) && get_user_weapon(terrorist) != CSW_C4)
    {
        Radar_ProbeCheck("bomb_given", true, "T inventory has C4");
        rg_drop_item(terrorist, "weapon_c4");
        g_ActorForward[0] = -120.0;
        Radar_ProbeNext(11, 3.0);
    }
    else if (g_ProbeStep == 11 && Radar_FindBombBox() && !get_member(terrorist, m_bHasC4) && now - g_ProbeStarted > 0.5)
    {
        Radar_ProbeCheck("bomb_dropped", g_DroppedMessages > 0, "real weaponbox and dropped BombDrop observed");
        Actors_ReleaseInput();
        Radar_ProbeNext(12, 6.0);
    }
    else if (g_ProbeStep == 12)
    {
        new box = Radar_FindBombBox();
        if (box)
        {
            new Float:origin[3];
            pev(box, pev_origin, origin);
            Actors_AimAt(0, origin);
            g_ActorAngles[0][0] = 0.0;
            g_ActorForward[0] = 120.0;
        }
        else if (get_member(terrorist, m_bHasC4))
        {
            Radar_ProbeCheck("bomb_picked_up", true, "Touch removed box and restored C4 inventory");
            Actors_ReleaseInput();
            Actors_Place(0, g_BombsiteOrigin);
            rg_internal_cmd(terrorist, "weapon_c4");
            Radar_ProbeNext(13, 4.0);
        }
    }
    else if (g_ProbeStep == 13)
    {
        new signals[UnifiedSignals];
        get_member(terrorist, m_signals, signals);
        if ((signals[US_State] & _:SIGNAL_BOMB) && (pev(terrorist, pev_flags) & FL_ONGROUND) && get_user_weapon(terrorist) == CSW_C4)
        {
            Radar_ProbeCheck("bombzone", true, "SIGNAL_BOMB, FL_ONGROUND and active C4");
            g_ActorButtons[0] = IN_ATTACK;
            Radar_ProbeNext(14, 10.0);
        }
    }
    else if (g_ProbeStep == 14 && Radar_FindBomb())
    {
        Radar_ProbeCheck("bomb_planted", g_PlantedMessages > 0 && !get_member(terrorist, m_bHasC4), "planted grenade and MSG_ALL planted BombDrop");
        Actors_ReleaseInput();
        new Float:bomb_origin[3], Float:actor_origin[3];
        pev(Radar_FindBomb(), pev_origin, bomb_origin);
        if (!Actors_FindNear(bomb_origin, actor_origin, 48.0))
        {
            Radar_ProbeFail("setup_failed_ct_defuse_hull");
            return;
        }
        Actors_Place(1, actor_origin);
        Actors_AimAt(1, bomb_origin);
        g_ActorButtons[1] = IN_USE;
        Radar_ProbeNext(15, 15.0);
    }
    else if (g_ProbeStep == 15 && !Radar_FindBomb())
    {
        if (!get_member_game(m_bBombDefused) || g_ClearMessages < 1)
        {
            Radar_ProbeFail("bomb_disappeared_without_defuse_and_broadcast_clear");
            return;
        }
        Radar_ProbeCheck("bomb_defused", true, "GameDLL defused state and MSG_ALL BombPickup");
        Actors_ReleaseInput();
        g_ProbeRestarted = false;
        rg_restart_round();
        Radar_ProbeNext(16, 5.0);
    }
    else if (g_ProbeStep == 16 && g_ProbeRestarted && Actors_AreJoined())
    {
        Radar_ProbeCheck("bomb_round_reset", Actors_Count() == 2 && !Radar_FindBomb() && !Radar_FindBombBox(), "two actors, no planted/dropped C4 after restart");
        rg_join_team(counter_terrorist, TEAM_SPECTATOR);
        Radar_ProbeNext(17, 5.0);
    }
    else if (g_ProbeStep == 17 && (get_member(counter_terrorist, m_afPhysicsFlags) & PFLAG_OBSERVER))
    {
        if (Actors_TryObserve(counter_terrorist, terrorist, g_ActorUserids[0]) == 1)
        {
            Radar_ProbeCheck("spectator_target", true, "TEAM_SPECTATOR, PFLAG_OBSERVER, IN_EYE and T userid target");
            rg_join_team(counter_terrorist, TEAM_CT);
            rg_round_respawn(counter_terrorist);
            set_pev(terrorist, pev_takedamage, DAMAGE_AIM);
            ExecuteHamB(Ham_TakeDamage, terrorist, counter_terrorist, counter_terrorist, 1000.0, DMG_GENERIC);
            Radar_ProbeNext(18, 10.0);
        }
    }
    else if (g_ProbeStep == 18 && !is_user_alive(terrorist) && (get_member(terrorist, m_afPhysicsFlags) & PFLAG_OBSERVER))
    {
        Radar_ProbeCheck("death_observer", get_member(terrorist, m_iTeam) == TEAM_TERRORIST, "damage/killed then observer while retaining T team");
        Actors_Remove();
        rg_restart_round();
        Radar_ProbeCheck("probe_finished", true, "drop/pickup/plant/defuse/reset, spectator and T death observed");
        g_Probe = Probe_None;
        Diagnostics_SetTrace(g_TraceBeforeProbe);
    }
}

Radar_ProbeHostage(Float:now)
{
    if (g_ProbeStep == 19 && Float:get_member(g_ActorSlots[1], m_flNextAttack) <= 0.0
        && (pev(g_ActorSlots[1], pev_flags) & FL_ONGROUND))
    {
        g_ActorButtons[1] = IN_USE;
        Radar_ProbeNext(20, 3.0);
    }
    else if (g_ProbeStep == 20)
    {
        if (now - g_ProbeStarted > 0.2)
        {
            g_ActorButtons[1] = 0;
        }
        if (cs_get_hostage_foll(g_ProbeHostage) == g_ActorSlots[1])
        {
            Radar_ProbeCheck("hostage_use", true, "native follower matches TEAM_CT after IN_USE");
            pev(g_ActorSlots[1], pev_origin, g_ProbeOrigin);
            g_ActorForward[1] = -120.0;
            Radar_ProbeNext(21, 5.0);
        }
    }
    else if (g_ProbeStep == 21)
    {
        new Float:origin[3];
        pev(g_ActorSlots[1], pev_origin, origin);
        if (get_distance_f(origin, g_ProbeOrigin) > 32.0)
        {
            Actors_ReleaseInput();
            g_ProbeRestarted = false;
            rg_restart_round();
            Radar_ProbeNext(22, 5.0);
        }
    }
    else if (g_ProbeStep == 22 && g_ProbeRestarted)
    {
        if (!Radar_SaveHostages(true))
        {
            Radar_ProbeFail("hostage_ids_changed_after_restart");
            return;
        }
        Radar_ProbeCheck("hostage_reset", true, "same hostage_id sequence and count after round restart");
        if (++g_ProbeRestartCount < 5)
        {
            g_ProbeRestarted = false;
            rg_restart_round();
            Radar_ProbeNext(22, 5.0);
        }
        else
        {
            Actors_Remove();
            Radar_ProbeCheck("probe_finished", true, "hostage IN_USE and five stable round resets");
            g_Probe = Probe_None;
            Diagnostics_SetTrace(g_TraceBeforeProbe);
        }
    }
}

public Radar_Message(message_id, destination, recipient)
{
    new message_name[32];
    get_user_msgname(message_id, message_name, charsmax(message_name));
    if (equal(message_name, "BombDrop") || equal(message_name, "BombPickup")) { Bomb_ObserveMessage(message_name, destination, recipient); }
    new bool:running = g_CoreState == Core_Running;
    if (equal(message_name, "HostagePos") || equal(message_name, "HostageK"))
    {
        new bool:periodic = equal(message_name, "HostageK") || get_msg_arg_int(1) == 0;
        if (running && periodic && (!recipient || !is_user_alive(recipient) || get_member(recipient, m_iTeam) != TEAM_CT)) { g_BadMessageAudience++; }
        if (equal(message_name, "HostagePos"))
        {
            Radar_ObserveHostageId();
            Hostage_FollowObserveMessage(recipient);
            if (get_msg_arg_int(1) == 1) { g_HostageInitialMessages++; }
            else if (get_msg_arg_int(2) == g_SceneHostageId) { g_HostageMoveMessages++; }
        }
        else if (get_msg_arg_int(1) == g_SceneHostageId) { g_HostageEndMessages++; }
    }
    else if (equal(message_name, "Radar") && running)
    {
        new source = get_msg_arg_int(1);
        if (!recipient || !is_user_alive(recipient) || !is_user_connected(source)
            || source == recipient || get_member(source, m_iTeam) != get_member(recipient, m_iTeam)) { g_BadMessageAudience++; }
        g_RadarMoveMessages++;
        for (new actor = 0; actor < 3; actor++)
        {
            if (Actors_IsCurrent(actor) && source == g_ActorSlots[actor]) { g_RadarActorMessages[actor]++; }
        }
    }
    if (equal(message_name, "BombDrop"))
    {
        if (get_msg_arg_int(4) == 1 && destination == MSG_ALL)
        {
            g_PlantedMessages++;
        }
        else if (get_msg_arg_int(4) == 0 && recipient && is_user_alive(recipient) && get_member(recipient, m_iTeam) == TEAM_TERRORIST)
        {
            g_DroppedMessages++;
        }
        else if (running) { g_BadMessageAudience++; }
    }
    else if (equal(message_name, "BombPickup") && destination == MSG_ALL)
    {
        g_ClearMessages++;
    }
    else if (equal(message_name, "BombPickup") && running
        && (!recipient || !is_user_alive(recipient) || get_member(recipient, m_iTeam) != TEAM_TERRORIST)) { g_BadMessageAudience++; }
    if (!Diagnostics_TraceSlot()) { return PLUGIN_CONTINUE; }
    new JSON:data = json_init_object();
    json_object_set_string(data, "name", message_name);
    json_object_set_number(data, "destination", destination);
    if (recipient)
    {
        json_object_set_number(data, "recipient_userid", get_user_userid(recipient));
    }
    else
    {
        json_object_set_null(data, "recipient_userid");
    }
    new JSON:eligible = json_init_array();
    if (!recipient)
    {
        for (new slot = 1; slot <= MaxClients; slot++)
        {
            if (is_user_connected(slot))
            {
                json_array_append_number(eligible, get_user_userid(slot));
            }
        }
    }
    json_object_set_value(data, "eligible_userids", eligible);
    json_free(eligible);
    new JSON:payload = json_init_array();
    for (new argument = 1; argument <= get_msg_args(); argument++)
    {
        new type = get_msg_argtype(argument);
        if (type == ARG_COORD || type == ARG_ANGLE)
        {
            json_array_append_real(payload, get_msg_arg_float(argument));
        }
        else
        {
            json_array_append_number(payload, get_msg_arg_int(argument));
        }
    }
    json_object_set_value(data, "payload", payload);
    json_free(payload);
    if (g_Stage >= 0) { json_object_set_real(data, "stage_elapsed", get_gametime() - g_StageStarted); }
    else { json_object_set_null(data, "stage_elapsed"); }
    Diagnostics_Emit("message", data);
    json_free(data);
    return PLUGIN_CONTINUE;
}

public Radar_Anchor(id, level, cid)
{
    #pragma unused level, cid
    if (!id || !is_user_connected(id) || read_argc() < 2)
    {
        return PLUGIN_HANDLED;
    }
    new name[32], map_name[32], path[128], hash[65];
    new Float:origin[3], Float:angles[3];
    read_argv(1, name, charsmax(name));
    for (new index = 0; name[index]; index++)
    {
        if (!((name[index] >= 'a' && name[index] <= 'z') || (name[index] >= '0' && name[index] <= '9') || name[index] == '_'))
        {
            console_print(id, "Anchor names use lowercase ASCII letters, digits and underscore");
            return PLUGIN_HANDLED;
        }
    }
    get_mapname(map_name, charsmax(map_name));
    formatex(path, charsmax(path), "maps/%s.bsp", map_name);
    hash_file(path, Hash_Sha256, hash, charsmax(hash));
    pev(id, pev_origin, origin);
    pev(id, pev_v_angle, angles);
    console_print(id, "[map]^nschema=1^nname=%s^nbsp_sha256=%s^n[anchor:%s]^nkind=player^norigin=%.3f %.3f %.3f^nangles=%.3f %.3f %.3f", map_name, hash, name, origin[0], origin[1], origin[2], angles[0], angles[1], angles[2]);
    return PLUGIN_HANDLED;
}


bool:Radar_FindRetreat(actor, entity, const viewer_name[])
{
    new Float:origin[3], Float:hostage[3], Float:viewer[3], Float:line[3];
    pev(g_ActorSlots[actor], pev_origin, origin);
    pev(entity, pev_origin, hostage);
    new anchor = Maps_FindAnchor(viewer_name);
    for (new axis = 0; axis < 3; axis++) { viewer[axis] = g_AnchorOrigins[anchor][axis]; line[axis] = hostage[axis] - viewer[axis]; }
    line[2] = 0.0;
    new Float:length = vector_length(line);
    if (length == 0.0) { return false; }
    new trace = create_tr2();
    for (new direction = 0; direction < 8; direction++)
    {
        g_SceneTarget[0] = origin[0] + floatcos(float(direction * 45), degrees) * 64.0;
        g_SceneTarget[1] = origin[1] + floatsin(float(direction * 45), degrees) * 64.0;
        g_SceneTarget[2] = origin[2];
        new Float:cross = (g_SceneTarget[0] - viewer[0]) * line[1] - (g_SceneTarget[1] - viewer[1]) * line[0];
        if (floatabs(cross) / length < 48.0 || get_distance_f(g_SceneTarget, hostage) < 48.0) { continue; }
        engfunc(EngFunc_TraceHull, origin, g_SceneTarget, DONT_IGNORE_MONSTERS, HULL_HUMAN, g_ActorSlots[actor], trace);
        new Float:fraction;
        get_tr2(trace, TR_flFraction, fraction);
        if (fraction == 1.0 && !get_tr2(trace, TR_StartSolid)) { free_tr2(trace); return true; }
    }
    free_tr2(trace);
    return false;
}
