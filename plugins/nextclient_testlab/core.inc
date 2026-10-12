enum CoreState
{
    Core_Idle,
    Core_Preparing,
    Core_Ready,
    Core_Running,
    Core_Finished,
    Core_Failed,
    Core_Canceled
};

enum ScenarioId
{
    Scenario_HostageStatic,
    Scenario_HostageFollow,
    Scenario_HostageVisibility,
    Scenario_HostageEnd,
    Scenario_BombCycle,
    Scenario_BombVisibility,
    Scenario_BombObserver,
    Scenario_Players,
    Scenario_Lifecycle,
    Scenario_Count
};

new const kScenarioNames[][] = {"radar_hostage_static", "radar_hostage_follow", "radar_hostage_visibility", "radar_hostage_end",
    "radar_bomb_cycle", "radar_bomb_visibility", "radar_bomb_observer", "radar_players", "radar_lifecycle"};
new const kScenarioMaps[][] = {"cs_office", "cs_office", "cs_office", "cs_office", "de_dust2", "de_dust2", "de_dust2", "", ""};
new const kScenarioRoles[] = {3, 1, 7, 1, 3, 3, 2, 3, 7};
new const kScenarioDefaultRoles[] = {1, 1, 1, 1, 2, 2, 2, 1, 1};
new const kScenarioSteps[] = {2, 4, 4, 1, 5, 4, 4, 4, 4};
new const kStateNames[][] = {"idle", "preparing", "ready", "running", "finished", "failed", "canceled"};

new CoreState:g_CoreState;
new g_Generation;
new g_RunCounter;
new g_RunId[64];
new g_LastRunId[64];
new ScenarioId:g_Scenario;
new g_Stage = -1;
new Float:g_StageStarted;
new Float:g_CoreDeadline;
new g_OperatorSlot;
new g_OperatorUserid;
new g_OperatorRole = 1;
new bool:g_OperatorSaved;
new Float:g_OperatorDamage;
new g_OperatorMoveType;
new g_ClientUserids[33];
new bool:g_Resetting;
new bool:g_ResetFinished;
new bool:g_ResetRestartIssued;
new bool:g_RepeatAfterReset;
new ScenarioId:g_RepeatScenario;
new g_RepeatRole;
new bool:g_ResetHostages;
new g_CvarPointers[16];
new g_CvarNames[16][48];
new g_CvarValues[16][128];
new g_CvarCount;
new g_PendingPath[256];
new bool:g_Pending;
new ScenarioId:g_PendingScenario;
new g_PendingRole;
new g_PendingExpires;
new g_MenuRole[33];
new g_Outcome[16] = "defuse";
new bool:g_HeadlessOperator;

bool:Core_IsOutcome(const outcome[])
{
    return equal(outcome, "defuse") || equal(outcome, "explosion") || equal(outcome, "kill")
        || equal(outcome, "rescue") || equal(outcome, "dropped") || equal(outcome, "planted");
}

bool:Core_OperatorMatches(slot, userid)
{
    return slot > 0 && slot == g_OperatorSlot && userid > 0 && userid == g_OperatorUserid;
}

bool:Core_IsOperatorCurrent()
{
    return g_OperatorSlot > 0 && is_user_connected(g_OperatorSlot)
        && Core_OperatorMatches(g_OperatorSlot, get_user_userid(g_OperatorSlot))
        && (!is_user_bot(g_OperatorSlot)
            || (g_HeadlessOperator && Actors_IsCurrent(3) && g_OperatorSlot == Actors_TechnicalSlot()));
}

bool:Core_ClaimOperator(id)
{
    if (!id)
    {
        return Core_IsOperatorCurrent();
    }
    if (!is_user_connected(id) || is_user_bot(id))
    {
        return false;
    }
    if (Core_IsOperatorCurrent() && !Core_OperatorMatches(id, get_user_userid(id)))
    {
        return false;
    }
    g_OperatorSlot = id;
    g_OperatorUserid = get_user_userid(id);
    if (!g_OperatorSaved)
    {
        pev(id, pev_takedamage, g_OperatorDamage);
        g_OperatorMoveType = pev(id, pev_movetype);
        g_OperatorSaved = true;
    }
    return true;
}

Core_Reply(id, const text[])
{
    console_print(id, "TestLab: %s", text);
    if (id)
    {
        client_print(id, print_chat, "TestLab: %s", text);
    }
}

bool:Core_IsCallbackCurrent(generation)
{
    return generation == g_Generation && (g_CoreState == Core_Preparing || g_CoreState == Core_Running);
}

bool:Core_CanStep()
{
    return !g_Resetting && g_CoreState == Core_Ready;
}

Core_Event(const name[], const code[] = "")
{
    new JSON:data = json_init_object();
    json_object_set_string(data, "scenario_id", kScenarioNames[_:g_Scenario]);
    json_object_set_string(data, "state", kStateNames[_:g_CoreState]);
    json_object_set_number(data, "operator_userid", g_OperatorUserid);
    json_object_set_string(data, "code", code);
    Diagnostics_Emit(name, data);
    json_free(data);
}

Core_StageReady(generation)
{
    if (generation != g_Generation || g_CoreState != Core_Preparing)
    {
        return;
    }
    Actors_ReleaseInput();
    g_CoreState = Core_Ready;
    Diagnostics_State("");
    Core_Event("stage_ready");
    Core_Reply(g_OperatorSlot, "ready; inspect the scene, then nct_step");
}

Core_StageFinished(generation)
{
    if (generation != g_Generation || g_CoreState != Core_Running)
    {
        return;
    }
    Actors_ReleaseInput();
    Diagnostics_State("");
    Core_Event("stage_finished");
    if (g_Stage + 1 == kScenarioSteps[_:g_Scenario])
    {
        g_CoreState = Core_Finished;
        Scenario_Stop(g_Scenario, generation);
        Core_Event("run_finished");
        Core_Reply(g_OperatorSlot, "finished; record nct_result and nct_report, or nct_repeat");
    }
    else
    {
        g_CoreState = Core_Ready;
        Core_Event("stage_ready");
        Core_Reply(g_OperatorSlot, "ready; inspect the scene, then nct_step");
    }
}

Core_Restore()
{
    for (new index = g_CvarCount - 1; index >= 0; index--)
    {
        set_pcvar_string(g_CvarPointers[index], g_CvarValues[index]);
    }
    g_CvarCount = 0;
    if (g_OperatorSaved && Core_IsOperatorCurrent())
    {
        set_pev(g_OperatorSlot, pev_takedamage, g_OperatorDamage);
        set_pev(g_OperatorSlot, pev_movetype, g_OperatorMoveType);
    }
    g_OperatorSaved = false;
}

bool:Core_SetCvar(const name[], const value[])
{
    new pointer = get_cvar_pointer(name);
    if (!pointer)
    {
        return false;
    }
    for (new index = 0; index < g_CvarCount; index++)
    {
        if (g_CvarPointers[index] == pointer)
        {
            set_pcvar_string(pointer, value);
            return true;
        }
    }
    if (g_CvarCount == sizeof g_CvarPointers)
    {
        return false;
    }
    g_CvarPointers[g_CvarCount] = pointer;
    copy(g_CvarNames[g_CvarCount], charsmax(g_CvarNames[]), name);
    get_pcvar_string(pointer, g_CvarValues[g_CvarCount], charsmax(g_CvarValues[]));
    g_CvarCount++;
    set_pcvar_string(pointer, value);
    return true;
}

Core_Stop(CoreState:target_state, const code[])
{
    new bool:active = g_CoreState == Core_Preparing || g_CoreState == Core_Ready || g_CoreState == Core_Running;
    new generation = g_Generation++;
    g_CoreState = target_state;
    g_Resetting = false;
    g_RepeatAfterReset = false;
    Actors_ReleaseInput();
    if (g_RunId[0] && active)
    {
        Diagnostics_State("");
        Core_Event(target_state == Core_Failed ? "run_failed" : "run_canceled", code);
    }
    Scenario_Stop(g_Scenario, generation);
    if (g_RunId[0]) { copy(g_LastRunId, charsmax(g_LastRunId), g_RunId); }
    g_RunId[0] = 0;
    g_Stage = -1;
    Actors_Remove();
    Core_Restore();
}

Core_Cancel(const code[])
{
    Core_Stop(Core_Canceled, code);
}

Core_Fail(generation, const code[])
{
    if (Core_IsCallbackCurrent(generation) || (generation == g_Generation && g_CoreState == Core_Ready))
    {
        Core_Stop(Core_Failed, code);
        Core_Reply(g_OperatorSlot, code);
    }
}

Core_OnDisconnect(slot, userid)
{
    if (Core_OperatorMatches(slot, userid))
    {
        Core_Cancel("operator_disconnected");
        g_OperatorSlot = 0;
        g_OperatorUserid = 0;
    }
}

public client_putinserver(id)
{
    g_ClientUserids[id] = get_user_userid(id);
    g_MenuRole[id] = 1;
}

public client_disconnected(id)
{
    Core_OnDisconnect(id, g_ClientUserids[id]);
    g_ClientUserids[id] = 0;
}

Core_RoundReset(bool:repeat, next_scenario = -1, next_role = 0)
{
    g_RepeatScenario = next_scenario < 0 ? g_Scenario : ScenarioId:next_scenario;
    g_RepeatRole = next_role ? next_role : g_OperatorRole;
    Core_Cancel("round_reset");
    g_ResetHostages = Radar_FindClass("hostage_entity") != 0;
    if (g_ResetHostages && !Radar_SaveHostages(false, false))
    {
        Core_Reply(g_OperatorSlot, "setup_failed_hostage_baseline");
        return;
    }
    g_RepeatAfterReset = repeat;
    g_Resetting = true;
    g_ResetFinished = false;
    g_ResetRestartIssued = false;
    g_CoreDeadline = get_gametime() + 5.0;
}

Core_Begin(ScenarioId:scenario, role)
{
    Core_Cancel("new_run");
    if (!Core_IsOperatorCurrent())
    {
        return;
    }
    Core_ClaimOperator(g_OperatorSlot);
    g_Scenario = scenario;
    g_OperatorRole = role;
    g_Stage = -1;
    g_CoreState = Core_Preparing;
    g_Generation++;
    formatex(g_RunId, charsmax(g_RunId), "%s.%d.%d", g_SessionId, g_MapEpoch, ++g_RunCounter);
    g_CoreDeadline = get_gametime() + 10.0;
    Diagnostics_ResetTrace();
    Core_Event("run_started");
    if (!Scenario_Prepare(scenario, g_OperatorSlot, g_Generation))
    {
        Core_Fail(g_Generation, "setup_failed_prepare");
    }
}

bool:Core_PendingIsValid(const session[], epoch, bool:map_matches, expires, now)
{
    return equal(session, g_SessionId) && epoch == g_MapEpoch && map_matches
        && expires >= now && expires <= now + 120;
}

Core_Initialize()
{
    get_datadir(g_PendingPath, charsmax(g_PendingPath));
    add(g_PendingPath, charsmax(g_PendingPath), "/nextclient_testlab/pending.ini");
    new file = fopen(g_PendingPath, "rt");
    if (!file)
    {
        return;
    }
    new lines[8][128];
    for (new index = 0; index < sizeof lines; index++)
    {
        fgets(file, lines[index], charsmax(lines[]));
        trim(lines[index]);
    }
    fclose(file);
    delete_file(g_PendingPath);
    new map_name[32];
    get_mapname(map_name, charsmax(map_name));
    g_PendingScenario = ScenarioId:str_to_num(lines[4][12]);
    g_PendingRole = str_to_num(lines[5][14]);
    g_PendingExpires = str_to_num(lines[6][11]);
    g_Pending = equal(lines[0], "schema=1") && equal(lines[1], "session_id=", 11)
        && equal(lines[2], "expected_map_epoch=", 19) && equal(lines[3], "map=", 4)
        && equal(lines[4], "scenario_id=", 12) && equal(lines[5], "operator_role=", 14)
        && equal(lines[6], "expires_at=", 11)
        && equal(lines[7], "outcome=", 8) && Core_IsOutcome(lines[7][8])
        && _:g_PendingScenario >= 0 && _:g_PendingScenario < _:Scenario_Count
        && (g_PendingRole == 1 || g_PendingRole == 2 || g_PendingRole == 4)
        && Core_PendingIsValid(lines[1][11], str_to_num(lines[2][19]), bool:equal(lines[3][4], map_name),
            g_PendingExpires, get_systime());
    if (g_Pending) { copy(g_Outcome, charsmax(g_Outcome), lines[7][8]); }
    Radar_ProbeCheck("pending_loaded", g_Pending, "pending consumed; resume requires a connected operator");
}

Core_ChangeMap(const map_name[], bool:pending, ScenarioId:scenario, role)
{
    Core_Cancel("map_change");
    delete_file(g_PendingPath);
    g_Pending = false;
    if (pending)
    {
        new file = fopen(g_PendingPath, "wt");
        if (!file)
        {
            Core_Reply(g_OperatorSlot, "setup_failed_pending_write");
            return;
        }
        fprintf(file, "schema=1^nsession_id=%s^nexpected_map_epoch=%d^nmap=%s^nscenario_id=%d^noperator_role=%d^nexpires_at=%d^noutcome=%s^n",
            g_SessionId, g_MapEpoch + 1, map_name, _:scenario, role, get_systime() + 120, g_Outcome);
        fclose(file);
    }
    server_cmd("changelevel %s", map_name);
}

Core_Tick(Float:now)
{
    if (g_Pending && get_systime() > g_PendingExpires)
    {
        g_Pending = false;
    }
    if (g_Resetting)
    {
        if (!g_ResetRestartIssued && !Actors_Count() && !Actors_HasPendingRemoval())
        {
            if (g_RepeatAfterReset && Core_IsOperatorCurrent())
            {
                new TeamName:team = g_RepeatRole == 1 ? TEAM_CT : g_RepeatRole == 2 ? TEAM_TERRORIST : TEAM_SPECTATOR;
                if (!Actors_EnsureTeam(g_OperatorSlot, team)) { g_Resetting = false; Core_Reply(g_OperatorSlot, "setup_failed_reset_operator_team"); return; }
            }
            g_ResetRestartIssued = true;
            g_ResetFinished = false;
            rg_restart_round();
        }
        if (now > g_CoreDeadline)
        {
            g_Resetting = false;
            Core_Reply(g_OperatorSlot, "setup_failed_round_reset_timeout");
        }
        else if (g_ResetFinished && Actors_Count() == 0 && !Actors_HasPendingRemoval())
        {
            // Joining from spectator finishes in PlayerPreThink, after RestartRound.
            if (g_RepeatAfterReset && Core_IsOperatorCurrent() && get_member(g_OperatorSlot, m_iJoiningState) != JOINED)
            {
                return;
            }
            g_Resetting = false;
            if ((g_ResetHostages && !Radar_SaveHostages(true, false)) || Radar_FindBomb() || Radar_FindBombBox())
            {
                Core_Reply(g_OperatorSlot, "setup_failed_round_reset_postconditions");
                return;
            }
            g_CoreState = Core_Idle;
            if (g_RepeatAfterReset)
            {
                Core_Begin(g_RepeatScenario, g_RepeatRole);
            }
            else
            {
                Core_Reply(g_OperatorSlot, "idle; round reset complete");
            }
        }
        return;
    }
    if (!Core_IsCallbackCurrent(g_Generation))
    {
        return;
    }
    if (!Core_IsOperatorCurrent())
    {
        Core_Cancel("operator_identity_changed");
        return;
    }
    if (now > g_CoreDeadline)
    {
        Core_Fail(g_Generation, "setup_failed_stage_timeout");
        return;
    }
    Scenario_Tick(g_Scenario, g_Generation, now);
}

public Core_Command(id, level, cid)
{
    #pragma unused level, cid
    new command[32], arguments[192];
    read_argv(0, command, charsmax(command));
    read_args(arguments, charsmax(arguments));
    trim(arguments);
    remove_quotes(arguments);
    Core_Dispatch(id, command, arguments);
    return PLUGIN_HANDLED;
}

Core_Dispatch(id, const command[], const arguments[])
{
    if (equal(command, "nct_list"))
    {
        for (new index = 0; index < _:Scenario_Count; index++)
        {
            console_print(id, "%s map=%s roles=%d steps=%d", kScenarioNames[index], kScenarioMaps[index], kScenarioRoles[index], kScenarioSteps[index]);
        }
        return;
    }
    if (equal(command, "nct_status"))
    {
        console_print(id, "TestLab: run=%s last_run=%s state=%s stage=%d generation=%d operator=%d reset=%d pending=%d", g_RunId, g_LastRunId, kStateNames[_:g_CoreState], g_Stage, g_Generation, g_OperatorUserid, g_Resetting, g_Pending);
        Diagnostics_State(arguments);
        return;
    }
    if (equal(command, "nct_report"))
    {
        Diagnostics_Report(arguments);
        return;
    }
    if (equal(command, "nct_trace"))
    {
        if (equal(arguments, "on") || equal(arguments, "off"))
        {
            Diagnostics_SetTrace(bool:equal(arguments, "on"));
            Core_Reply(id, "trace setting applied; nct_report includes completeness");
        }
        else { Core_Reply(id, "usage: nct_trace on|off"); }
        return;
    }
    if (equal(command, "nct_operator") && !id)
    {
        new userid = str_to_num(arguments);
        for (new slot = 1; slot <= MaxClients; slot++)
        {
            if (is_user_connected(slot) && get_user_userid(slot) == userid && Core_ClaimOperator(slot))
            {
                Core_Reply(0, "operator selected by current userid");
                return;
            }
        }
        Core_Reply(0, "connected human userid required");
        return;
    }
    if (!Core_ClaimOperator(id))
    {
        Core_Reply(id, "current operator required; server console: nct_operator <userid>");
        return;
    }
    new map_name[32];
    get_mapname(map_name, charsmax(map_name));
    if (equal(command, "nct_run"))
    {
        new scenario_name[48], role_name[16];
        parse(arguments, scenario_name, charsmax(scenario_name), role_name, charsmax(role_name));
        for (new index = 0; index < _:Scenario_Count; index++)
        {
            if (equal(scenario_name, kScenarioNames[index]))
            {
                new role = !role_name[0] ? kScenarioDefaultRoles[index] : equal(role_name, "ct") ? 1 : equal(role_name, "t") ? 2 : equal(role_name, "observer") ? 4 : 0;
                if (!(kScenarioRoles[index] & role))
                {
                    Core_Reply(id, "invalid scenario role");
                    return;
                }
                g_Pending = false;
                delete_file(g_PendingPath);
                if (kScenarioMaps[index][0] && !equal(kScenarioMaps[index], map_name))
                {
                    Core_ChangeMap(kScenarioMaps[index], true, ScenarioId:index, role);
                }
                else
                {
                    Core_RoundReset(true, index, role);
                }
                return;
            }
        }
        Core_Reply(id, "unknown scenario; nct_list");
    }
    else if (equal(command, "nct_step"))
    {
        if (!Core_CanStep())
        {
            Core_Reply(id, g_CoreState == Core_Running ? "busy" : "stage is not ready");
            return;
        }
        g_CoreState = Core_Running;
        g_Stage++;
        g_StageStarted = get_gametime();
        g_CoreDeadline = g_StageStarted + 20.0;
        Core_Event("stage_started");
        if (!Scenario_StartStep(g_Scenario, g_Stage, g_Generation))
        {
            Core_Fail(g_Generation, "setup_failed_start_step");
        }
    }
    else if (equal(command, "nct_repeat"))
    {
        if (!g_RunId[0] && !g_LastRunId[0])
        {
            Core_Reply(id, "no scenario to repeat");
            return;
        }
        if (g_Scenario == Scenario_Lifecycle)
        {
            Core_ChangeMap(map_name, true, g_Scenario, g_OperatorRole);
        }
        else
        {
            Core_RoundReset(true);
        }
    }
    else if (equal(command, "nct_reset"))
    {
        if (equal(arguments, "round"))
        {
            Core_RoundReset(false);
        }
        else if (equal(arguments, "map"))
        {
            Core_ChangeMap(map_name, false, g_Scenario, g_OperatorRole);
        }
        else
        {
            Core_Reply(id, "usage: nct_reset round|map");
        }
    }
    else if (equal(command, "nct_resume"))
    {
        if (!g_Pending || get_systime() > g_PendingExpires)
        {
            g_Pending = false;
            Core_Reply(id, "no valid pending request for this map");
            return;
        }
        g_Pending = false;
        Core_RoundReset(true, _:g_PendingScenario, g_PendingRole);
    }
    else if (equal(command, "nct_menu"))
    {
        Core_Menu(id);
    }
    else if (equal(command, "nct_map"))
    {
        if (!equal(arguments, "cs_office") && !equal(arguments, "de_dust2")) { Core_Reply(id, "usage: nct_map cs_office|de_dust2"); return; }
        Core_ChangeMap(arguments, true, Scenario_Lifecycle, g_OperatorRole);
    }
    else if (equal(command, "nct_outcome"))
    {
        if (Core_IsOutcome(arguments))
        {
            copy(g_Outcome, charsmax(g_Outcome), arguments);
            Core_Reply(id, "outcome selected for the next action/run");
        }
        else
        {
            Core_Reply(id, "usage: nct_outcome defuse|explosion|kill|rescue|dropped|planted");
        }
    }
    else if (equal(command, "nct_view"))
    {
        new entity = Radar_CurrentObjective();
        Core_Reply(id, entity && Diagnostics_StartView(arguments, entity) ? "LOS matches; inspect nct_status visibility after three seconds" : "LOS mismatch or missing view; nct_status records PVS separately");
    }
    else if (equal(command, "nct_result"))
    {
        new result[16], note[160];
        argbreak(arguments, result, charsmax(result), note, charsmax(note));
        if ((!g_RunId[0] && !g_LastRunId[0]) || !(equal(result, "pass") || equal(result, "fail") || equal(result, "skip")) || !note[0])
        {
            Core_Reply(id, "usage: nct_result pass|fail|skip <note>; requires a run");
            return;
        }
        new JSON:data = json_init_object();
        json_object_set_string(data, "status", result);
        json_object_set_string(data, "note", note);
        json_object_set_number(data, "operator_userid", g_OperatorUserid);
        Diagnostics_Emit("manual_result", data, g_RunId[0] ? g_RunId : g_LastRunId);
        json_free(data);
    }
    else
    {
        Actors_OperatorAction(g_OperatorSlot, command, arguments);
    }
}

Core_Menu(id)
{
    if (!id)
    {
        return;
    }
    new menu = menu_create("NextClient TestLab", "Core_MenuSelected");
    menu_additem(menu, "Role: CT", "role 1");
    menu_additem(menu, "Role: T", "role 2");
    menu_additem(menu, "Role: Observer", "role 4");
    for (new index = 0; index < _:Scenario_Count; index++)
    {
        if (kScenarioRoles[index] & g_MenuRole[id])
        {
            new action[64];
            formatex(action, charsmax(action), "run %d", index);
            menu_additem(menu, kScenarioNames[index], action);
        }
    }
    menu_additem(menu, "Next stage", "step");
    menu_additem(menu, "Repeat", "repeat");
    menu_additem(menu, "Reset round", "round");
    menu_additem(menu, "Reset map", "map");
    menu_additem(menu, "Resume after map change", "resume");
    menu_additem(menu, "Outcome: defuse", "defuse");
    menu_additem(menu, "Outcome: explosion", "explosion");
    menu_additem(menu, "Outcome: hostage kill", "kill");
    menu_additem(menu, "Outcome: hostage rescue", "rescue");
    menu_additem(menu, "Visibility: dropped C4", "dropped");
    menu_additem(menu, "Visibility: planted C4", "planted");
    menu_additem(menu, "Respawn", "respawn");
    menu_additem(menu, "Normal death", "die");
    menu_additem(menu, "Godmode on", "god on");
    menu_additem(menu, "Godmode off", "god off");
    menu_additem(menu, "Noclip on", "noclip on");
    menu_additem(menu, "Noclip off", "noclip off");
    menu_display(id, menu);
}

public Core_MenuSelected(id, menu, item)
{
    new action[64], label[64], access, callback;
    if (item < 0 || !menu_item_getinfo(menu, item, access, action, charsmax(action), label, charsmax(label), callback))
    {
        menu_destroy(menu);
        return PLUGIN_HANDLED;
    }
    menu_destroy(menu);
    if (equal(action, "role ", 5))
    {
        g_MenuRole[id] = str_to_num(action[5]);
        Core_Menu(id);
        return PLUGIN_HANDLED;
    }
    if (equal(action, "run ", 4))
    {
        new arguments[64], index = str_to_num(action[4]);
        formatex(arguments, charsmax(arguments), "%s %s", kScenarioNames[index], g_MenuRole[id] == 1 ? "ct" : g_MenuRole[id] == 2 ? "t" : "observer");
        Core_Dispatch(id, "nct_run", arguments);
    }
    else if (equal(action, "round") || equal(action, "map"))
    {
        Core_Dispatch(id, "nct_reset", action);
    }
    else if (equal(action, "defuse") || equal(action, "explosion") || equal(action, "kill") || equal(action, "rescue") || equal(action, "dropped") || equal(action, "planted"))
    {
        Core_Dispatch(id, "nct_outcome", action);
    }
    else
    {
        new command[32], arguments[32];
        parse(action, command, charsmax(command), arguments, charsmax(arguments));
        new prefixed[36];
        formatex(prefixed, charsmax(prefixed), "nct_%s", command);
        Core_Dispatch(id, prefixed, arguments);
    }
    return PLUGIN_HANDLED;
}

bool:Scenario_Prepare(ScenarioId:scenario, operator_id, generation)
{
    return Radar_Prepare(scenario, operator_id, generation);
}

bool:Scenario_StartStep(ScenarioId:scenario, step_index, generation)
{
    return Radar_StartStep(scenario, step_index, generation);
}

Scenario_Tick(ScenarioId:scenario, generation, Float:now)
{
    Radar_Tick(scenario, generation, now);
}

Scenario_Stop(ScenarioId:scenario, generation)
{
    Radar_Stop(scenario, generation);
}
