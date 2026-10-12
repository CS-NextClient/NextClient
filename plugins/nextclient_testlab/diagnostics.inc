new g_Sequence;
new g_EventPath[256];
new g_EventPayload[4097];
new g_EventLine[4106];
new bool:g_TraceEnabled;
new g_TraceCount;
new bool:g_TraceTruncated;
new g_WatchedEntity;
new g_WatchedView = -1;
new Float:g_VisibilityDeadline;
new bool:g_VisibilityObserved;
new bool:g_VisibilityPvs;
new g_VisibilityPack;
new g_VisibilityLosHit;
new bool:g_VisibilityLos;
new bool:g_VisibilityReported;
new Float:g_VisibilityTime;
new Float:g_VisibilityTraceTime;

bool:Diagnostics_TraceSlot()
{
    if (!g_TraceEnabled || g_TraceTruncated) { return false; }
    if (g_TraceCount++ < 10000) { return true; }
    g_TraceTruncated = true;
    new JSON:data = json_init_object();
    json_object_set_string(data, "id", "trace_limit");
    json_object_set_string(data, "scope", "server");
    json_object_set_string(data, "status", "not_run");
    json_object_set_string(data, "expected", "complete diagnostic trace");
    json_object_set_string(data, "actual", "trace truncated after 10000 events; absence is not evidence");
    Diagnostics_Emit("check", data);
    json_free(data);
    return false;
}

Diagnostics_SetTrace(bool:enabled)
{
    g_TraceEnabled = enabled;
}

Diagnostics_StopView()
{
    g_WatchedEntity = 0;
    g_WatchedView = -1;
    g_VisibilityObserved = false;
    g_VisibilityReported = false;
    g_VisibilityLos = false;
    g_VisibilityLosHit = 0;
    g_VisibilityPack = 0;
    g_VisibilityTime = 0.0;
}

Diagnostics_Emit(const event_name[], JSON:data, const run_override[] = "")
{
    new JSON:event = json_init_object();
    json_object_set_number(event, "schema", 1);
    json_object_set_string(event, "event", event_name);
    json_object_set_string(event, "session_id", g_SessionId);
    json_object_set_number(event, "map_epoch", g_MapEpoch);
    json_object_set_number(event, "seq", ++g_Sequence);
    if (run_override[0])
    {
        json_object_set_string(event, "run_id", run_override);
    }
    else if (g_RunId[0])
    {
        json_object_set_string(event, "run_id", g_RunId);
    }
    else
    {
        json_object_set_null(event, "run_id");
    }
    if (g_Stage >= 0)
    {
        json_object_set_number(event, "stage", g_Stage);
    }
    else
    {
        json_object_set_null(event, "stage");
    }
    json_object_set_real(event, "server_time", get_gametime());
    json_object_set_value(event, "data", data);
    if (json_serial_size(event) > 4096)
    {
        json_free(event);
        set_fail_state("Protocol event exceeds 4096 bytes");
        return;
    }
    json_serial_to_string(event, g_EventPayload, charsmax(g_EventPayload));
    json_free(event);
    new file = fopen(g_EventPath, "at");
    if (!file)
    {
        set_fail_state("Cannot open TestLab event journal");
        return;
    }
    fprintf(file, "NCTEST %s^n", g_EventPayload);
    fclose(file);
    formatex(g_EventLine, charsmax(g_EventLine), "NCTEST %s^n", g_EventPayload);
    engfunc(EngFunc_ServerPrint, g_EventLine);
}

Diagnostics_Initialize()
{
    get_localinfo("nct_session_id", g_SessionId, charsmax(g_SessionId));
    if (strlen(g_SessionId) != 32)
    {
        set_fail_state("Host session_id is required");
        return;
    }
    for (new index = 0; index < 32; index++)
    {
        if (!((g_SessionId[index] >= '0' && g_SessionId[index] <= '9') || (g_SessionId[index] >= 'a' && g_SessionId[index] <= 'f')))
        {
            set_fail_state("Invalid host session_id");
            return;
        }
    }
    new directory[192];
    get_datadir(directory, charsmax(directory));
    add(directory, charsmax(directory), "/nextclient_testlab");
    if (!dir_exists(directory))
    {
        mkdir(directory);
    }
    new epoch_path[256];
    formatex(epoch_path, charsmax(epoch_path), "%s/epoch.txt", directory);
    new file = fopen(epoch_path, "rt");
    new previous_session[40], epoch[16];
    if (file)
    {
        fgets(file, previous_session, charsmax(previous_session));
        trim(previous_session);
        fgets(file, epoch, charsmax(epoch));
        fclose(file);
    }
    g_MapEpoch = equal(previous_session, g_SessionId) ? str_to_num(epoch) + 1 : 1;
    file = fopen(epoch_path, "wt");
    if (!file)
    {
        set_fail_state("Cannot save map epoch");
        return;
    }
    fprintf(file, "%s^n%d^n", g_SessionId, g_MapEpoch);
    fclose(file);
    get_localinfo("amxx_logs", directory, charsmax(directory));
    add(directory, charsmax(directory), "/nextclient_testlab");
    if (!dir_exists(directory))
    {
        mkdir(directory);
    }
    formatex(g_EventPath, charsmax(g_EventPath), "%s/%s.jsonl", directory, g_SessionId);
    new const modules[][] = {"cstrike", "fakemeta", "hamsandwich", "reapi", "json"};
    new JSON:data = json_init_object();
    new JSON:loaded = json_init_array();
    for (new index = 0; index < sizeof modules; index++)
    {
        if (!LibraryExists(modules[index], LibType_Library))
        {
            json_free(loaded);
            json_free(data);
            set_fail_state("Required AMXX module unavailable");
            return;
        }
        json_array_append_string(loaded, modules[index]);
    }
    new map_name[32];
    get_mapname(map_name, charsmax(map_name));
    json_object_set_string(data, "map", map_name);
    json_object_set_string(data, "plugin_version", kPluginVersion);
    json_object_set_value(data, "modules", loaded);
    json_free(loaded);
    Diagnostics_Emit("server_ready", data);
    json_free(data);
}

public Diagnostics_Session()
{
    server_print("NCTSESSION %s", g_SessionId);
    return PLUGIN_HANDLED;
}

Diagnostics_ResetTrace()
{
    g_TraceCount = 0;
    g_TraceTruncated = false;
    Diagnostics_StopView();
}

Diagnostics_State(const request_id[])
{
    new JSON:data = json_init_object();
    json_object_set_string(data, "request_id", request_id);
    if (g_LastRunId[0]) { json_object_set_string(data, "last_run_id", g_LastRunId); }
    else { json_object_set_null(data, "last_run_id"); }
    json_object_set_string(data, "scenario_id", kScenarioNames[_:g_Scenario]);
    json_object_set_string(data, "state", kStateNames[_:g_CoreState]);
    json_object_set_string(data, "outcome", g_Outcome);
    new map_name[32];
    get_mapname(map_name, charsmax(map_name));
    json_object_set_string(data, "map", map_name);
    json_object_set_number(data, "generation", g_Generation);
    if (g_MapHash[0]) { json_object_set_string(data, "map_hash", g_MapHash); }
    else { json_object_set_null(data, "map_hash"); }
    json_object_set_bool(data, "trace_truncated", g_TraceTruncated);
    json_object_set_string(data, "hostage_id_source", Radar_UsesHostageIdAdapter() ? "observed_message_coordinate_match" : "amxx_native");
    new JSON:visibility = json_init_object();
    json_object_set_number(visibility, "entity_index", g_WatchedEntity);
    json_object_set_string(visibility, "pvs", g_VisibilityObserved ? (g_VisibilityPvs ? "inside" : "outside") : "unknown");
    json_object_set_string(visibility, "los", !g_WatchedEntity ? "unknown" : g_VisibilityLos ? "clear" : "blocked");
    json_object_set_number(visibility, "los_hit", g_VisibilityLosHit);
    if (g_VisibilityObserved) { json_object_set_number(visibility, "original_pack", g_VisibilityPack); }
    else { json_object_set_null(visibility, "original_pack"); }
    json_object_set_value(data, "visibility", visibility);
    json_free(visibility);
    if (g_Scenario == Scenario_HostageFollow)
    {
        new JSON:follow = Hostage_FollowState();
        json_object_set_value(data, "hostage_follow", follow);
        json_free(follow);
    }
    new JSON:actors = Actors_StateJson();
    json_object_set_value(data, "actors", actors);
    json_free(actors);
    new JSON:objectives = Radar_ObjectivesJson();
    json_object_set_value(data, "objectives", objectives);
    json_free(objectives);
    new JSON:cvars = json_init_object();
    for (new index = 0; index < g_CvarCount; index++)
    {
        new value[128];
        get_pcvar_string(g_CvarPointers[index], value, charsmax(value));
        json_object_set_string(cvars, g_CvarNames[index], value);
    }
    json_object_set_value(data, "cvars", cvars);
    json_free(cvars);
    Diagnostics_Emit("state", data);
    json_free(data);
}

Diagnostics_Report(const request_id[])
{
    Diagnostics_State(request_id);
    new JSON:data = json_init_object();
    json_object_set_string(data, "request_id", request_id);
    json_object_set_number(data, "last_seq", g_Sequence + 1);
    json_object_set_bool(data, "complete", !g_TraceTruncated);
    json_object_set_bool(data, "trace_truncated", g_TraceTruncated);
    Diagnostics_Emit("report_end", data);
    json_free(data);
}

bool:Diagnostics_StartView(const name[], entity)
{
    g_WatchedView = Maps_FindView(name);
    if (g_WatchedView < 0 || !pev_valid(entity)) { return false; }
    g_WatchedEntity = entity;
    g_VisibilityObserved = false;
    g_VisibilityReported = false;
    g_VisibilityTime = 0.0;
    g_VisibilityDeadline = get_gametime() + 3.0;
    Diagnostics_RefreshLos();
    return g_VisibilityLos == g_ViewLos[g_WatchedView];
}


Diagnostics_RefreshLos()
{
    new Float:eyes[3], Float:offset[3], Float:target[3], Float:size[3];
    pev(g_OperatorSlot, pev_origin, eyes);
    pev(g_OperatorSlot, pev_view_ofs, offset);
    pev(g_WatchedEntity, pev_absmin, target);
    pev(g_WatchedEntity, pev_size, size);
    for (new axis = 0; axis < 3; axis++)
    {
        eyes[axis] += offset[axis];
        target[axis] += size[axis] * 0.5;
    }
    new trace = create_tr2();
    engfunc(EngFunc_TraceLine, eyes, target, DONT_IGNORE_MONSTERS, g_OperatorSlot, trace);
    new Float:fraction;
    get_tr2(trace, TR_flFraction, fraction);
    g_VisibilityLosHit = get_tr2(trace, TR_pHit);
    g_VisibilityLos = fraction == 1.0 || g_VisibilityLosHit == g_WatchedEntity;
    free_tr2(trace);
}

Diagnostics_ViewResult()
{
    if (g_WatchedView < 0) { return -1; }
    if (g_HeadlessOperator)
    {
        if (g_VisibilityReported) { return 2; }
        g_VisibilityReported = true;
        new JSON:data = json_init_object();
        json_object_set_string(data, "id", "view_pvs");
        json_object_set_string(data, "scope", "server");
        json_object_set_string(data, "status", "not_run");
        json_object_set_string(data, "expected", g_ViewPvs[g_WatchedView] ? "inside" : "outside");
        json_object_set_string(data, "actual", "unknown; no real AddToFullPack client");
        Diagnostics_Emit("check", data);
        json_free(data);
        return 2;
    }
    if (!g_VisibilityObserved) { return get_gametime() > g_VisibilityDeadline ? -1 : 0; }
    return g_VisibilityPvs == g_ViewPvs[g_WatchedView] ? 1 : get_gametime() > g_VisibilityDeadline ? -1 : 0;
}

public Diagnostics_FullPack(es, entity_number, entity, host, host_flags, player, p_set)
{
    #pragma unused es, entity_number, host_flags, player
    if (entity != g_WatchedEntity || !Core_OperatorMatches(host, get_user_userid(host)) || is_user_bot(host) || !p_set)
    {
        return FMRES_IGNORED;
    }
    new bool:previous_pvs = g_VisibilityPvs, previous_pack = g_VisibilityPack;
    g_VisibilityObserved = true;
    g_VisibilityPvs = bool:engfunc(EngFunc_CheckVisibility, entity, p_set);
    g_VisibilityPack = get_orig_retval();
    g_VisibilityTime = get_gametime();
    Hostage_FollowObserveVisibility(g_VisibilityPvs, g_VisibilityPack, g_VisibilityTime);
    new bool:record = g_Scenario != Scenario_HostageFollow || g_VisibilityTime - g_VisibilityTraceTime >= 0.5
        || previous_pvs != g_VisibilityPvs || previous_pack != g_VisibilityPack;
    if (record)
    {
        g_VisibilityTraceTime = g_VisibilityTime;
        if (g_Scenario == Scenario_HostageFollow) { Diagnostics_RefreshLos(); }
    }
    if (record && Diagnostics_TraceSlot())
    {
        new JSON:data = json_init_object();
        json_object_set_number(data, "entity_index", entity);
        json_object_set_number(data, "viewer_userid", get_user_userid(host));
        json_object_set_string(data, "pvs", g_VisibilityPvs ? "inside" : "outside");
        json_object_set_number(data, "original_pack", g_VisibilityPack);
        json_object_set_string(data, "los", g_VisibilityLos ? "clear" : "blocked");
        json_object_set_number(data, "los_hit", g_VisibilityLosHit);
        json_object_set_number(data, "observer_mode", pev(host, pev_iuser1));
        json_object_set_real(data, "stage_elapsed", get_gametime() - g_StageStarted);
        Diagnostics_Emit("visibility", data);
        json_free(data);
    }
    return FMRES_IGNORED;
}
