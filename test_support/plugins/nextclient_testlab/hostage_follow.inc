new bool:g_FollowInside;
new bool:g_FollowOutside;
new bool:g_FollowReturned;
new bool:g_FollowHeadlessChecks[3];
new Float:g_FollowInsideSince;
new Float:g_FollowOutsideSince;
new Float:g_FollowOutsideSeconds;
new Float:g_FollowOutsideDistance;
new Float:g_FollowMessagePosition[3];
new bool:g_FollowHasMessagePosition;
new g_FollowOutsideMessages;
new g_FollowOperatorMessages;
new g_FollowMessagesBefore;
new Float:g_FollowViewerOrigin[3];

bool:Hostage_FollowVisibilityMatches(bool:pvs, pack, bool:inside, Float:age)
{
    return age >= 0.0 && age <= 0.5 && pvs == inside && pack == (inside ? 1 : 0);
}

Float:Hostage_FollowLeaderDistance(const Float:hostage[3], const Float:leader[3], Float:leader_min_z)
{
    new Float:feet[3];
    for (new axis = 0; axis < 3; axis++) { feet[axis] = leader[axis]; }
    feet[2] += leader_min_z;
    return get_distance_f(hostage, feet);
}

bool:Hostage_FollowOutsideReady(Float:seconds, messages, Float:distance)
{
    return seconds >= 6.0 && messages >= 3 && distance >= 256.0;
}

Hostage_FollowResetOutside()
{
    g_FollowOutsideSince = 0.0;
    g_FollowOutsideSeconds = 0.0;
    g_FollowOutsideDistance = 0.0;
    g_FollowOutsideMessages = 0;
    g_FollowHasMessagePosition = false;
}

Hostage_FollowReset()
{
    g_FollowInside = false;
    g_FollowOutside = false;
    g_FollowReturned = false;
    g_FollowInsideSince = 0.0;
    g_FollowOperatorMessages = 0;
    for (new index = 0; index < sizeof g_FollowHeadlessChecks; index++) { g_FollowHeadlessChecks[index] = false; }
    Hostage_FollowResetOutside();
}

Hostage_FollowObserveVisibility(bool:pvs, pack, Float:now)
{
    if (g_Scenario != Scenario_HostageFollow || !Core_IsCallbackCurrent(g_SceneGeneration)
        || g_CoreState != Core_Running || g_HeadlessOperator) { return; }
    if (g_Stage == 0 || g_Stage == 2)
    {
        if (!Hostage_FollowVisibilityMatches(pvs, pack, true, 0.0)
            || cs_get_hostage_foll(g_SceneHostage) != g_ActorSlots[1])
        {
            g_FollowInsideSince = 0.0;
            return;
        }
        if (!g_FollowInsideSince) { g_FollowInsideSince = now; }
        if (now - g_FollowInsideSince < 0.5) { return; }
        if (g_Stage == 0 && !g_FollowInside)
        {
            g_FollowInside = true;
            Radar_ProbeCheck("hostage_follow_inside", true, "selected moving hostage: PVS inside and original_pack=1");
        }
        else if (g_Stage == 2 && g_FollowOutside && !g_FollowReturned)
        {
            g_FollowReturned = true;
            Radar_ProbeCheck("hostage_follow_return", true, "selected moving hostage returned to PVS inside and original_pack=1");
        }
    }
    else if (g_Stage == 1 && !g_FollowOutside)
    {
        if (!Hostage_FollowVisibilityMatches(pvs, pack, false, 0.0)) { Hostage_FollowResetOutside(); return; }
        if (!g_FollowOutsideSince) { g_FollowOutsideSince = now; }
        g_FollowOutsideSeconds = now - g_FollowOutsideSince;
        if (Hostage_FollowOutsideReady(g_FollowOutsideSeconds, g_FollowOutsideMessages, g_FollowOutsideDistance))
        {
            g_FollowOutside = true;
            new actual[192];
            formatex(actual, charsmax(actual), "PVS outside and original_pack=0 for %.1f seconds; %d selected HostagePos to operator; distance=%.1f",
                g_FollowOutsideSeconds, g_FollowOutsideMessages, g_FollowOutsideDistance);
            Radar_ProbeCheck("hostage_follow_outside", true, actual);
        }
    }
}

Hostage_FollowObserveMessage(recipient)
{
    if (g_Scenario != Scenario_HostageFollow || g_CoreState != Core_Running
        || get_msg_arg_int(1) != 0 || get_msg_arg_int(2) != g_SceneHostageId || recipient != g_OperatorSlot) { return; }
    g_FollowOperatorMessages++;
    if (g_HeadlessOperator || g_Stage != 1 || g_FollowOutside || !g_FollowOutsideSince
        || !Hostage_FollowVisibilityMatches(g_VisibilityPvs, g_VisibilityPack, false, get_gametime() - g_VisibilityTime)) { return; }
    new Float:origin[3];
    for (new axis = 0; axis < 3; axis++) { origin[axis] = get_msg_arg_float(axis + 3); }
    if (g_FollowHasMessagePosition) { g_FollowOutsideDistance += get_distance_f(origin, g_FollowMessagePosition); }
    for (new axis = 0; axis < 3; axis++) { g_FollowMessagePosition[axis] = origin[axis]; }
    g_FollowHasMessagePosition = true;
    g_FollowOutsideMessages++;
}

bool:Hostage_FollowGate(step_index)
{
    if (g_HeadlessOperator)
    {
        if (!g_FollowHeadlessChecks[step_index])
        {
            new const names[][] = {"hostage_follow_inside", "hostage_follow_outside", "hostage_follow_return"};
            new JSON:data = json_init_object();
            json_object_set_string(data, "id", names[step_index]);
            json_object_set_string(data, "scope", "server");
            json_object_set_string(data, "status", "not_run");
            json_object_set_string(data, "expected", "PVS and original_pack observed for the connected operator");
            json_object_set_string(data, "actual", "dedicated server operations with technical actor");
            Diagnostics_Emit("check", data);
            json_free(data);
            g_FollowHeadlessChecks[step_index] = true;
        }
        return true;
    }
    if (!Hostage_FollowVisibilityMatches(g_VisibilityPvs, g_VisibilityPack, step_index != 1,
        get_gametime() - g_VisibilityTime)) { return false; }
    return step_index == 0 ? g_FollowInside : step_index == 1 ? g_FollowOutside : g_FollowReturned;
}

Hostage_FollowTick(Float:now)
{
    if (g_CoreState != Core_Running) { return; }
    if (now - g_VisibilityTime > 0.5)
    {
        g_FollowInsideSince = 0.0;
        if (!g_FollowOutside) { Hostage_FollowResetOutside(); }
    }
    new Float:origin[3];
    pev(g_OperatorSlot, pev_origin, origin);
    if (get_distance_f(origin, g_FollowViewerOrigin) > 8.0) { Core_Fail(g_SceneGeneration, "setup_failed_follow_viewer_moved"); }
}

JSON:Hostage_FollowState()
{
    new JSON:data = json_init_object();
    new const phases[][] = {"use_inside", "outbound", "return_inside", "stop"};
    json_object_set_string(data, "phase", g_Stage < 0 ? "prepared" : phases[g_Stage]);
    json_object_set_bool(data, "inside_confirmed", g_FollowInside);
    json_object_set_bool(data, "outside_confirmed", g_FollowOutside);
    json_object_set_bool(data, "return_confirmed", g_FollowReturned);
    json_object_set_real(data, "outside_seconds", g_FollowOutsideSeconds);
    json_object_set_real(data, "outside_distance", g_FollowOutsideDistance);
    json_object_set_number(data, "outside_messages", g_FollowOutsideMessages);
    json_object_set_number(data, "operator_messages", g_FollowOperatorMessages);
    json_object_set_number(data, "waypoint", g_SceneWaypoint);
    if (g_SceneRoute >= 0) { json_object_set_string(data, "route", g_RouteNames[g_SceneRoute]); }
    else { json_object_set_null(data, "route"); }
    return data;
}
