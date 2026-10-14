new bool:g_GeometryPending;
new Float:g_GeometryDeadline;
new Float:g_GeometryViewOffset[3];
new Float:g_GeometryFeetOffset;
new Float:g_GeometryTarget[3];

bool:Geometry_IsRunning()
{
    return g_GeometryPending;
}

public Geometry_Command()
{
    if (Core_IsCallbackCurrent(g_Generation) || g_Resetting || g_Probe != Probe_None || Actors_Count(true) || Geometry_IsRunning())
    {
        server_print("TestLab: busy");
        return PLUGIN_HANDLED;
    }
    if (!Actors_Create(3, TEAM_CT))
    {
        Radar_ProbeCheck("geometry_finished", false, "cannot create geometry actor");
        return PLUGIN_HANDLED;
    }
    g_GeometryPending = true;
    g_GeometryDeadline = get_gametime() + 5.0;
    return PLUGIN_HANDLED;
}

bool:Geometry_ClearSegment(const Float:start[3], const Float:finish[3])
{
    new trace = create_tr2();
    engfunc(EngFunc_TraceHull, start, finish, IGNORE_MONSTERS, HULL_HUMAN, 0, trace);
    new Float:fraction;
    get_tr2(trace, TR_flFraction, fraction);
    new bool:clear = fraction == 1.0 && !get_tr2(trace, TR_StartSolid) && !get_tr2(trace, TR_AllSolid);
    free_tr2(trace);
    return clear;
}

bool:Geometry_Los(const Float:origin[3])
{
    new Float:eyes[3];
    for (new axis = 0; axis < 3; axis++) { eyes[axis] = origin[axis] + g_GeometryViewOffset[axis]; }
    new trace = create_tr2();
    engfunc(EngFunc_TraceLine, eyes, g_GeometryTarget, IGNORE_MONSTERS, 0, trace);
    new Float:fraction;
    get_tr2(trace, TR_flFraction, fraction);
    free_tr2(trace);
    return fraction == 1.0;
}

bool:Geometry_BlockedEnvelope(const Float:origin[3])
{
    new Float:eyes[3], Float:target[3], trace = create_tr2(), bool:blocked = true;
    for (new axis = 0; axis < 3; axis++) { eyes[axis] = origin[axis] + g_GeometryViewOffset[axis]; }
    // The actor arrival radius and objective height must not turn a wall-edge sample into clear LOS.
    for (new corner = 0; corner < 8; corner++)
    {
        target[0] = g_GeometryTarget[0] + (corner & 1 ? 16.0 : -16.0);
        target[1] = g_GeometryTarget[1] + (corner & 2 ? 16.0 : -16.0);
        target[2] = g_GeometryTarget[2] + (corner & 4 ? 36.0 : 0.0);
        engfunc(EngFunc_TraceLine, eyes, target, IGNORE_MONSTERS, 0, trace);
        new Float:fraction;
        get_tr2(trace, TR_flFraction, fraction);
        if (fraction == 1.0) { blocked = false; break; }
    }
    free_tr2(trace);
    return blocked;
}

Geometry_WriteAnchor(file, const name[], const kind[], const Float:origin[3])
{
    new Float:direction[3], Float:angles[3];
    for (new axis = 0; axis < 3; axis++) { direction[axis] = g_GeometryTarget[axis] - origin[axis] - g_GeometryViewOffset[axis]; }
    engfunc(EngFunc_VecToAngles, direction, angles);
    angles[0] = -angles[0];
    while (angles[0] < -180.0) { angles[0] += 360.0; }
    fprintf(file, "^n[anchor:%s]^nkind=%s^norigin=%.3f %.3f %.3f^nangles=%.3f %.3f %.3f^n", name, kind,
        origin[0], origin[1], origin[2], angles[0], angles[1], angles[2]);
}

bool:Geometry_FindBlocked(const Float:target[3], Float:origin[3])
{
    new Float:point[3];
    for (new radius = 128; radius <= 512; radius += 128)
    {
        for (new direction = 0; direction < 16; direction++)
        {
            point[0] = target[0] + floatcos(float(direction) * 22.5, degrees) * float(radius);
            point[1] = target[1] + floatsin(float(direction) * 22.5, degrees) * float(radius);
            point[2] = target[2];
            if (Actors_FindFloor(point, origin) && Geometry_BlockedEnvelope(origin)) { return true; }
        }
    }
    return false;
}

bool:Geometry_FindOutside(Float:origin[3])
{
    new Float:best_distance, bool:found;
    new const classes[][] = {"info_player_start", "info_player_deathmatch"};
    for (new index = 0; index < sizeof classes; index++)
    {
        new entity;
        while ((entity = Radar_FindClass(classes[index], entity)))
        {
            new Float:point[3], Float:candidate[3];
            pev(entity, pev_origin, point);
            if (!Actors_FindFloor(point, candidate) || Geometry_Los(candidate)) { continue; }
            new Float:distance = get_distance_f(candidate, g_GeometryTarget);
            if (distance > best_distance)
            {
                best_distance = distance;
                for (new axis = 0; axis < 3; axis++) { origin[axis] = candidate[axis]; }
                found = true;
            }
        }
    }
    return found;
}

bool:Geometry_WriteOffice(file)
{
    new entity = Radar_FindClass("hostage_entity");
    if (!entity) { return false; }
    new Float:hostage[3], Float:minimum[3], Float:size[3], Float:use[3], Float:viewer[3];
    pev(entity, pev_origin, hostage);
    pev(entity, pev_absmin, minimum);
    pev(entity, pev_size, size);
    for (new axis = 0; axis < 3; axis++) { g_GeometryTarget[axis] = minimum[axis] + size[axis] * 0.5; }
    if (!Actors_FindNear(hostage, use, 48.0) || !Geometry_Los(use)) { return false; }
    Geometry_WriteAnchor(file, "hostage_start", "hostage", hostage);
    Geometry_WriteAnchor(file, "ct_use", "player", use);
    Geometry_WriteAnchor(file, "ct_route_start", "player", use);
    if (!Actors_FindNear(hostage, viewer, 128.0) || !Geometry_Los(viewer)) { return false; }
    Geometry_WriteAnchor(file, "viewer_open", "player", viewer);
    new rescue = Radar_FindClass("func_hostage_rescue");
    if (rescue)
    {
        new Float:maximum[3], Float:point[3], Float:entry[3], Float:start[3], Float:ct_use[3];
        pev(rescue, pev_absmin, minimum);
        pev(rescue, pev_absmax, maximum);
        for (new axis = 0; axis < 3; axis++) { point[axis] = (minimum[axis] + maximum[axis]) * 0.5; }
        if (Actors_FindFloor(point, entry))
        {
            Geometry_WriteAnchor(file, "rescue_entry", "player", entry);
            for (new direction = 0; direction < 8; direction++)
            {
                point[0] = entry[0] + floatcos(float(direction * 45), degrees) * (floatmax(maximum[0] - minimum[0], maximum[1] - minimum[1]) * 0.5 + 128.0);
                point[1] = entry[1] + floatsin(float(direction * 45), degrees) * (floatmax(maximum[0] - minimum[0], maximum[1] - minimum[1]) * 0.5 + 128.0);
                point[2] = entry[2];
                if (!Actors_FindFloor(point, start) || !Geometry_ClearSegment(start, entry)) { continue; }
                start[2] += g_GeometryFeetOffset;
                if (!Actors_FindNear(start, ct_use, 48.0) || !Geometry_ClearSegment(ct_use, entry)) { continue; }
                Geometry_WriteAnchor(file, "rescue_hostage_start", "hostage", start);
                Geometry_WriteAnchor(file, "rescue_ct_use", "player", ct_use);
                break;
            }
        }
    }
    return Geometry_WriteViews(file, hostage, "hostage_start");
}

bool:Geometry_WriteViews(file, const Float:target[3], const target_name[])
{
    new Float:occluded[3], Float:outside[3];
    if (Geometry_FindBlocked(target, occluded))
    {
        Geometry_WriteAnchor(file, "viewer_occluded", "player", occluded);
        fprintf(file, "^n[view:occluded]^nviewer=viewer_occluded^ntarget=%s^nlos=blocked^npvs=inside^n", target_name);
    }
    if (Geometry_FindOutside(outside))
    {
        Geometry_WriteAnchor(file, "viewer_outside", "player", outside);
        fprintf(file, "^n[view:outside]^nviewer=viewer_outside^ntarget=%s^nlos=blocked^npvs=outside^n", target_name);
    }
    fprintf(file, "^n[view:open]^nviewer=viewer_open^ntarget=%s^nlos=clear^npvs=inside^n", target_name);
    return true;
}

bool:Geometry_WriteDust2(file)
{
    new Float:plant[3], Float:defuse[3], Float:viewer[3];
    if (!Radar_FindBombsite(plant)) { return false; }
    for (new axis = 0; axis < 3; axis++) { g_GeometryTarget[axis] = plant[axis]; }
    g_GeometryTarget[2] += g_GeometryFeetOffset;
    if (!Actors_FindNear(g_GeometryTarget, defuse, 48.0) || !Actors_FindNear(g_GeometryTarget, viewer, 128.0)) { return false; }
    Geometry_WriteAnchor(file, "t_start", "player", plant);
    Geometry_WriteAnchor(file, "bombsite_plant", "player", plant);
    Geometry_WriteAnchor(file, "ct_defuse", "player", defuse);
    Geometry_WriteAnchor(file, "viewer_open", "player", viewer);
    Geometry_WriteAnchor(file, "bomb_drop", "point", g_GeometryTarget);
    return Geometry_WriteViews(file, g_GeometryTarget, "bomb_drop");
}

Geometry_Tick(Float:now)
{
    if (!g_GeometryPending) { return; }
    if (now > g_GeometryDeadline)
    {
        g_GeometryPending = false;
        Actors_Remove(true);
        Radar_ProbeCheck("geometry_finished", false, "geometry actor did not join");
        return;
    }
    if (!Actors_IsJoined(3)) { return; }
    new Float:minimum[3];
    pev(g_ActorSlots[3], pev_view_ofs, g_GeometryViewOffset);
    pev(g_ActorSlots[3], pev_mins, minimum);
    g_GeometryFeetOffset = minimum[2];
    new map_name[32], path[256], hash[65];
    get_mapname(map_name, charsmax(map_name));
    formatex(path, charsmax(path), "maps/%s.bsp", map_name);
    hash_file(path, Hash_Sha256, hash, charsmax(hash));
    get_datadir(path, charsmax(path));
    format(path, charsmax(path), "%s/nextclient_testlab/geometry_%s.ini", path, map_name);
    new file = fopen(path, "wt");
    new bool:valid;
    if (file)
    {
        fprintf(file, "; Hull and LOS checked on the server. PVS requires a connected real client.^n[map]^nschema=1^nname=%s^nbsp_sha256=%s^n", map_name, hash);
        valid = equal(map_name, "cs_office") ? Geometry_WriteOffice(file) : equal(map_name, "de_dust2") ? Geometry_WriteDust2(file) : false;
        fclose(file);
    }
    g_GeometryPending = false;
    Actors_Remove(true);
    Radar_ProbeCheck("geometry_finished", valid, valid ? "candidate profile saved; PVS unknown" : "setup_failed_geometry; calibrate with nct_anchor");
}
