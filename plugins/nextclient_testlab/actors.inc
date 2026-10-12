new g_ActorSlots[4];
new g_ActorUserids[4];
new g_PendingKickUserids[4];
new Float:g_ActorAngles[4][3];
new Float:g_ActorForward[4];
new g_ActorButtons[4];
new Float:g_ActorMsec[4];
new Float:g_ActorLastTick;
new TeamName:g_ActorTeams[4];
new g_ActorJoinStage[4];
new g_ObserveTarget;
new g_ObserveTargetUserid;
new g_ObserveOperatorUserid;
new g_ObserveGeneration;
new Float:g_ObserveDeadline;
new const kActorNames[][] = {"nct_t1", "nct_ct1", "nct_t2", "nct_probe_operator"};

bool:Actors_IsCurrent(actor)
{
    new slot = g_ActorSlots[actor];
    return slot > 0 && is_user_connected(slot) && get_user_userid(slot) == g_ActorUserids[actor] && is_user_bot(slot);
}

Actors_TechnicalSlot()
{
    return g_ActorSlots[3];
}

bool:Actors_EnsureTeam(id, TeamName:team)
{
    return get_member(id, m_iTeam) == team || bool:rg_join_team(id, team);
}

bool:Actors_Create(actor, TeamName:team)
{
    for (new slot = 1; slot <= MaxClients; slot++)
    {
        if (is_user_connected(slot))
        {
            new name[32];
            get_user_name(slot, name, charsmax(name));
            if (equal(name, kActorNames[actor]))
            {
                return false;
            }
        }
    }
    new slot = engfunc(EngFunc_CreateFakeClient, kActorNames[actor]);
    if (!slot)
    {
        return false;
    }
    g_ActorSlots[actor] = slot;
    g_ActorUserids[actor] = get_user_userid(slot);
    new reject[128];
    if (!dllfunc(DLLFunc_ClientConnect, slot, kActorNames[actor], "127.0.0.1", reject))
    {
        server_cmd("kick #%d", g_ActorUserids[actor]);
        return false;
    }
    dllfunc(DLLFunc_ClientPutInServer, slot);
    dllfunc(DLLFunc_ClientUserInfoChanged, slot, engfunc(EngFunc_GetInfoKeyBuffer, slot));
    if (!Actors_IsCurrent(actor))
    {
        return false;
    }
    g_ActorTeams[actor] = team;
    g_ActorJoinStage[actor] = 0;
    g_ActorLastTick = get_gametime();
    return true;
}

Actors_ReleaseInput()
{
    for (new actor = 0; actor < sizeof g_ActorSlots; actor++)
    {
        g_ActorButtons[actor] = 0;
        g_ActorForward[actor] = 0.0;
    }
}

Actors_Remove(bool:include_operator = false)
{
    Actors_ReleaseInput();
    for (new actor = 0; actor < (include_operator ? sizeof g_ActorSlots : 3); actor++)
    {
        if (Actors_IsCurrent(actor))
        {
            g_PendingKickUserids[actor] = g_ActorUserids[actor];
            server_cmd("kick #%d", g_ActorUserids[actor]);
        }
        g_ActorSlots[actor] = 0;
        g_ActorUserids[actor] = 0;
        g_ActorMsec[actor] = 0.0;
    }
}

bool:Actors_HasPendingRemoval()
{
    new bool:pending;
    for (new actor = 0; actor < sizeof g_PendingKickUserids; actor++)
    {
        if (!g_PendingKickUserids[actor]) { continue; }
        new bool:connected;
        for (new slot = 1; slot <= MaxClients; slot++)
        {
            if (is_user_connected(slot) && get_user_userid(slot) == g_PendingKickUserids[actor])
            {
                connected = true;
                break;
            }
        }
        if (connected) { pending = true; }
        else { g_PendingKickUserids[actor] = 0; }
    }
    return pending;
}

Actors_Count(bool:include_operator = false)
{
    new count;
    for (new actor = 0; actor < (include_operator ? sizeof g_ActorSlots : 3); actor++)
    {
        if (Actors_IsCurrent(actor))
        {
            count++;
        }
    }
    return count;
}

bool:Actors_AreJoined()
{
    for (new actor = 0; actor < 2; actor++)
    {
        if (!Actors_IsJoined(actor))
        {
            return false;
        }
    }
    return true;
}

bool:Actors_IsJoined(actor)
{
    if (!Actors_IsCurrent(actor) || get_member(g_ActorSlots[actor], m_iJoiningState) != JOINED)
    {
        return false;
    }
    if (!is_user_alive(g_ActorSlots[actor]))
    {
        rg_round_respawn(g_ActorSlots[actor]);
    }
    return bool:is_user_alive(g_ActorSlots[actor]);
}

bool:Actors_Tick(Float:now)
{
    Actors_ObserverTick(now);
    new Float:elapsed = now - g_ActorLastTick;
    g_ActorLastTick = now;
    if (elapsed < 0.0 || elapsed > 0.25)
    {
        Actors_ReleaseInput();
        return false;
    }
    for (new actor = 0; actor < sizeof g_ActorSlots; actor++)
    {
        if (!Actors_IsCurrent(actor))
        {
            continue;
        }
        new msec = Actors_AccumulateMsec(elapsed, g_ActorMsec[actor]);
        if (msec < 0)
        {
            return false;
        }
        if (msec < 1)
        {
            continue;
        }
        // Fake clients never receive clientdata, which normally clears the spawn angle lock.
        set_pev(g_ActorSlots[actor], pev_fixangle, 0);
        engfunc(EngFunc_RunPlayerMove, g_ActorSlots[actor], g_ActorAngles[actor], g_ActorForward[actor], 0.0, 0.0,
                g_ActorButtons[actor], 0, msec);
        Radar_InputFrame(actor, msec);
        if (g_ActorJoinStage[actor] == 0 && get_member(g_ActorSlots[actor], m_iJoiningState) == PICKINGTEAM)
        {
            rg_internal_cmd(g_ActorSlots[actor], "jointeam", g_ActorTeams[actor] == TEAM_TERRORIST ? "1" : "2");
            if (get_member(g_ActorSlots[actor], m_iMenu) != Menu_ChooseAppearance)
            {
                return false;
            }
            rg_internal_cmd(g_ActorSlots[actor], "joinclass", "1");
            g_ActorJoinStage[actor] = 1;
        }
    }
    return true;
}

Actors_AccumulateMsec(Float:elapsed, &Float:remainder)
{
    if (elapsed < 0.0 || elapsed > 0.25)
    {
        remainder = 0.0;
        return -1;
    }
    remainder += elapsed * 1000.0;
    new msec = floatround(remainder, floatround_floor);
    if (msec > 100)
    {
        remainder = 0.0;
        return -1;
    }
    remainder -= float(msec);
    return msec;
}

Actors_TryObserve(id, target, userid)
{
    if (!(get_member(id, m_afPhysicsFlags) & PFLAG_OBSERVER) || !is_user_alive(target) || get_user_userid(target) != userid) { return -1; }
    if (pev(id, pev_iuser1) == OBS_IN_EYE && pev(id, pev_iuser2) == target) { return 1; }
    if (Float:get_member(id, m_flNextFollowTime) > get_gametime()) { return 0; }
    new name[32];
    get_user_name(target, name, charsmax(name));
    // SetMode can select a target and start the GameDLL follow cooldown.
    rg_observer_find_next_player(id, false, name);
    rg_set_observer_mode(id, OBS_IN_EYE);
    return pev(id, pev_iuser1) == OBS_IN_EYE && pev(id, pev_iuser2) == target ? 1 : -1;
}

Actors_ObserverTick(Float:now)
{
    if (!g_ObserveTarget) { return; }
    if (!Core_IsOperatorCurrent() || g_OperatorUserid != g_ObserveOperatorUserid || g_Generation != g_ObserveGeneration)
    {
        g_ObserveTarget = 0;
        return;
    }
    new result = Actors_TryObserve(g_OperatorSlot, g_ObserveTarget, g_ObserveTargetUserid);
    if (!result && now < g_ObserveDeadline) { return; }
    g_ObserveTarget = 0;
    Core_Reply(g_OperatorSlot, result == 1 ? "observer target confirmed" : "setup_failed_observer_target");
}

Actors_OperatorAction(id, const command[], const arguments[])
{
    if (equal(command, "nct_team"))
    {
        new TeamName:team = equal(arguments, "ct") ? TEAM_CT : equal(arguments, "t") ? TEAM_TERRORIST : equal(arguments, "observer") ? TEAM_SPECTATOR : TEAM_UNASSIGNED;
        if (team == TEAM_UNASSIGNED || !Actors_EnsureTeam(id, team))
        {
            Core_Reply(id, "usage: nct_team ct|t|observer; requires completed joining");
        }
    }
    else if (equal(command, "nct_respawn"))
    {
        if (get_member(id, m_iTeam) == TEAM_CT || get_member(id, m_iTeam) == TEAM_TERRORIST)
        {
            rg_round_respawn(id);
        }
    }
    else if (equal(command, "nct_die"))
    {
        set_pev(id, pev_takedamage, DAMAGE_AIM);
        ExecuteHamB(Ham_TakeDamage, id, id, id, 10000.0, DMG_GENERIC);
    }
    else if (equal(command, "nct_god") || equal(command, "nct_noclip"))
    {
        if (!equal(arguments, "on") && !equal(arguments, "off"))
        {
            Core_Reply(id, "argument on|off required");
            return;
        }
        if (equal(command, "nct_god"))
        {
            set_pev(id, pev_takedamage, equal(arguments, "on") ? DAMAGE_NO : DAMAGE_AIM);
        }
        else
        {
            set_pev(id, pev_movetype, equal(arguments, "on") ? MOVETYPE_NOCLIP : MOVETYPE_WALK);
        }
    }
    else if (equal(command, "nct_observe"))
    {
        new userid = str_to_num(arguments);
        if (!(get_member(id, m_afPhysicsFlags) & PFLAG_OBSERVER))
        {
            Core_Reply(id, "first nct_team observer, then nct_observe <target userid>");
            return;
        }
        for (new slot = 1; slot <= MaxClients; slot++)
        {
            if (is_user_alive(slot) && get_user_userid(slot) == userid)
            {
                g_ObserveTarget = slot;
                g_ObserveTargetUserid = userid;
                g_ObserveOperatorUserid = get_user_userid(id);
                g_ObserveGeneration = g_Generation;
                g_ObserveDeadline = get_gametime() + 1.0;
                Actors_ObserverTick(get_gametime());
                return;
            }
        }
        Core_Reply(id, "living target userid required");
    }
    else if (equal(command, "nct_goto"))
    {
        if (g_Resetting || Core_IsCallbackCurrent(g_Generation)) { Core_Reply(id, "busy"); return; }
        Core_Reply(id, (g_RunId[0] || Maps_CheckProfile()) && Maps_PlacePlayer(id, arguments) ? "anchor reached" : "setup_failed_profile_anchor_or_hull");
    }
}

JSON:Actors_PlayerJson(id, const role[])
{
    new JSON:actor = json_init_object();
    json_object_set_string(actor, "role", role);
    json_object_set_number(actor, "slot", id);
    json_object_set_number(actor, "userid", get_user_userid(id));
    json_object_set_number(actor, "team", _:get_member(id, m_iTeam));
    json_object_set_number(actor, "joining_state", _:get_member(id, m_iJoiningState));
    json_object_set_bool(actor, "alive", bool:is_user_alive(id));
    json_object_set_number(actor, "buttons", pev(id, pev_button));
    json_object_set_number(actor, "flags", pev(id, pev_flags));
    new Float:next_attack;
    next_attack = Float:get_member(id, m_flNextAttack);
    json_object_set_real(actor, "next_attack", next_attack);
    new Float:origin[3], Float:angles[3], Float:health;
    pev(id, pev_origin, origin);
    pev(id, pev_v_angle, angles);
    pev(id, pev_health, health);
    json_object_set_real(actor, "health", health);
    new JSON:pose = json_init_array(), JSON:view = json_init_array();
    for (new axis = 0; axis < 3; axis++)
    {
        json_array_append_real(pose, origin[axis]);
        json_array_append_real(view, angles[axis]);
    }
    json_object_set_value(actor, "origin", pose);
    json_object_set_value(actor, "view_angles", view);
    json_free(pose);
    json_free(view);
    json_object_set_number(actor, "observer_mode", pev(id, pev_iuser1));
    new target = pev(id, pev_iuser2);
    if (target > 0 && target <= MaxClients && is_user_connected(target))
    {
        json_object_set_number(actor, "observer_target_userid", get_user_userid(target));
    }
    else
    {
        json_object_set_null(actor, "observer_target_userid");
    }
    return actor;
}

JSON:Actors_StateJson()
{
    new JSON:actors = json_init_array();
    for (new actor = 0; actor < 3; actor++)
    {
        if (Actors_IsCurrent(actor))
        {
            new JSON:state_data = Actors_PlayerJson(g_ActorSlots[actor], kActorNames[actor]);
            json_array_append_value(actors, state_data);
            json_free(state_data);
        }
    }
    if (Core_IsOperatorCurrent())
    {
        new JSON:state_data = Actors_PlayerJson(g_OperatorSlot, "operator");
        json_array_append_value(actors, state_data);
        json_free(state_data);
    }
    return actors;
}

Actors_AimAt(actor, const Float:target[3])
{
    new Float:eyes[3], Float:view_offset[3], Float:direction[3];
    pev(g_ActorSlots[actor], pev_origin, eyes);
    pev(g_ActorSlots[actor], pev_view_ofs, view_offset);
    for (new axis = 0; axis < 3; axis++)
    {
        direction[axis] = target[axis] - eyes[axis] - view_offset[axis];
    }
    engfunc(EngFunc_VecToAngles, direction, g_ActorAngles[actor]);
    g_ActorAngles[actor][0] = -g_ActorAngles[actor][0];
}

Actors_Place(actor, const Float:origin[3])
{
    new Float:zero[3];
    set_pev(g_ActorSlots[actor], pev_velocity, zero);
    engfunc(EngFunc_SetOrigin, g_ActorSlots[actor], origin);
}

bool:Actors_FindFloor(const Float:point[3], Float:origin[3])
{
    new trace = create_tr2();
    new Float:top[3], Float:bottom[3];
    for (new axis = 0; axis < 3; axis++)
    {
        top[axis] = point[axis];
        bottom[axis] = point[axis];
    }
    top[2] += 32.0;
    bottom[2] -= 1024.0;
    engfunc(EngFunc_TraceLine, top, bottom, IGNORE_MONSTERS, 0, trace);
    new Float:fraction;
    get_tr2(trace, TR_flFraction, fraction);
    get_tr2(trace, TR_vecEndPos, origin);
    if (fraction >= 1.0 || get_tr2(trace, TR_StartSolid))
    {
        free_tr2(trace);
        return false;
    }
    origin[2] += 36.1;
    engfunc(EngFunc_TraceHull, origin, origin, IGNORE_MONSTERS, HULL_HUMAN, 0, trace);
    new bool:clear = !get_tr2(trace, TR_StartSolid) && !get_tr2(trace, TR_AllSolid);
    free_tr2(trace);
    return clear;
}

bool:Actors_FindNear(const Float:point[3], Float:origin[3], Float:distance = 64.0)
{
    new Float:candidate[3];
    for (new index = 0; index < 8; index++)
    {
        candidate[0] = point[0] + floatcos(float(index * 45), degrees) * distance;
        candidate[1] = point[1] + floatsin(float(index * 45), degrees) * distance;
        candidate[2] = point[2];
        if (Actors_FindFloor(candidate, origin))
        {
            return true;
        }
    }
    return false;
}

bool:Actors_FindMove(actor, Float:distance = 64.0)
{
    new Float:origin[3], Float:target[3];
    pev(g_ActorSlots[actor], pev_origin, origin);
    new trace = create_tr2();
    for (new direction = 0; direction < 8; direction++)
    {
        target[0] = origin[0] + floatcos(float(direction * 45), degrees) * distance;
        target[1] = origin[1] + floatsin(float(direction * 45), degrees) * distance;
        target[2] = origin[2];
        engfunc(EngFunc_TraceHull, origin, target, IGNORE_MONSTERS, HULL_HUMAN, g_ActorSlots[actor], trace);
        new Float:fraction;
        get_tr2(trace, TR_flFraction, fraction);
        if (fraction == 1.0 && !get_tr2(trace, TR_StartSolid))
        {
            g_ActorAngles[actor][0] = 0.0;
            g_ActorAngles[actor][1] = float(direction * 45);
            g_ActorForward[actor] = 120.0;
            free_tr2(trace);
            return true;
        }
    }
    free_tr2(trace);
    return false;
}
