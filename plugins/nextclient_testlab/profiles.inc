enum AnchorKind {Anchor_Player, Anchor_Hostage, Anchor_Point};
new g_AnchorCount;
new g_AnchorNames[32][32];
new AnchorKind:g_AnchorKinds[32];
new Float:g_AnchorOrigins[32][3];
new Float:g_AnchorAngles[32][3];
new g_AnchorFields[32];
new g_RouteCount;
new g_RouteNames[4][32];
new g_RouteReferences[4][256];
new g_RouteAnchors[4][32];
new g_RouteLengths[4];
new Float:g_RouteSpeeds[4];
new Float:g_RouteRadii[4];
new Float:g_RouteTimeouts[4];
new g_RouteFields[4];
new g_ViewCount;
new g_ViewNames[8][32];
new g_ViewReferences[8][2][32];
new g_ViewAnchors[8][2];
new bool:g_ViewLos[8];
new bool:g_ViewPvs[8];
new g_ViewFields[8];

bool:Maps_IsName(const name[])
{
    if (!name[0] || strlen(name) > 31 || name[0] < 'a' || name[0] > 'z')
    {
        return false;
    }
    for (new index = 1; name[index]; index++)
    {
        if (!((name[index] >= 'a' && name[index] <= 'z') || (name[index] >= '0' && name[index] <= '9') || name[index] == '_'))
        {
            return false;
        }
    }
    return true;
}

bool:Maps_Number(const text[], &Float:value, Float:maximum = 65536.0)
{
    new digits, dots;
    for (new index = 0; text[index]; index++)
    {
        if (text[index] >= '0' && text[index] <= '9')
        {
            digits++;
        }
        else if (text[index] == '.' && ++dots <= 1)
        {
            continue;
        }
        else if (index != 0 || (text[index] != '-' && text[index] != '+'))
        {
            return false;
        }
    }
    value = str_to_float(text);
    return digits > 0 && floatabs(value) <= maximum;
}

bool:Maps_Vector(const text[], Float:vector[3], Float:maximum = 65536.0)
{
    new items[4][48];
    if (parse(text, items[0], charsmax(items[]), items[1], charsmax(items[]), items[2], charsmax(items[]), items[3], charsmax(items[])) != 3)
    {
        return false;
    }
    return Maps_Number(items[0], vector[0], maximum) && Maps_Number(items[1], vector[1], maximum) && Maps_Number(items[2], vector[2], maximum);
}

Maps_FindAnchor(const name[])
{
    for (new index = 0; index < g_AnchorCount; index++)
    {
        if (equal(name, g_AnchorNames[index]))
        {
            return index;
        }
    }
    return -1;
}

Maps_FindRoute(const name[])
{
    for (new index = 0; index < g_RouteCount; index++)
    {
        if (equal(name, g_RouteNames[index]))
        {
            return index;
        }
    }
    return -1;
}

Maps_FindView(const name[])
{
    for (new index = 0; index < g_ViewCount; index++)
    {
        if (equal(name, g_ViewNames[index]))
        {
            return index;
        }
    }
    return -1;
}

bool:Maps_ReadProfile()
{
    new map_name[32], path[256], line[512], expected_hash[65];
    get_mapname(map_name, charsmax(map_name));
    get_configsdir(path, charsmax(path));
    format(path, charsmax(path), "%s/nextclient_testlab/maps/%s.ini", path, map_name);
    new file = fopen(path, "rt");
    if (!file)
    {
        return false;
    }
    g_AnchorCount = 0;
    g_RouteCount = 0;
    g_ViewCount = 0;
    new section, item, fields, bool:valid = true;
    while (!feof(file) && valid)
    {
        fgets(file, line, charsmax(line));
        if (strlen(line) == charsmax(line))
        {
            valid = false;
            break;
        }
        trim(line);
        if (!line[0] || line[0] == ';')
        {
            continue;
        }
        if (line[0] == '[')
        {
            new length = strlen(line);
            if (line[length - 1] != ']')
            {
                valid = false;
                break;
            }
            line[length - 1] = 0;
            if (equal(line[1], "map") && !fields)
            {
                section = 1;
                fields = 8;
            }
            else if (equal(line[1], "anchor:", 7) && g_AnchorCount < sizeof g_AnchorNames
                && Maps_IsName(line[8]) && Maps_FindAnchor(line[8]) == -1)
            {
                section = 2;
                item = g_AnchorCount++;
                copy(g_AnchorNames[item], charsmax(g_AnchorNames[]), line[8]);
                g_AnchorFields[item] = 0;
            }
            else if (equal(line[1], "route:", 6) && g_RouteCount < sizeof g_RouteNames
                && Maps_IsName(line[7]) && Maps_FindRoute(line[7]) == -1)
            {
                section = 3;
                item = g_RouteCount++;
                copy(g_RouteNames[item], charsmax(g_RouteNames[]), line[7]);
                g_RouteFields[item] = 0;
                g_RouteLengths[item] = 0;
            }
            else if (equal(line[1], "view:", 5) && g_ViewCount < sizeof g_ViewNames
                && Maps_IsName(line[6]) && Maps_FindView(line[6]) == -1)
            {
                section = 4;
                item = g_ViewCount++;
                copy(g_ViewNames[item], charsmax(g_ViewNames[]), line[6]);
                g_ViewFields[item] = 0;
            }
            else
            {
                valid = false;
            }
            continue;
        }
        new equals = contain(line, "="), key[48], value[384], bit;
        if (equals < 1)
        {
            valid = false;
            break;
        }
        line[equals] = 0;
        copy(key, charsmax(key), line);
        copy(value, charsmax(value), line[equals + 1]);
        trim(key);
        trim(value);
        if (section == 1)
        {
            if (equal(key, "schema") && equal(value, "1")) { bit = 1; }
            else if (equal(key, "name") && equal(value, map_name)) { bit = 2; }
            else if (equal(key, "bsp_sha256") && strlen(value) == 64)
            {
                copy(expected_hash, charsmax(expected_hash), value);
                bit = 4;
            }
            valid = bit != 0 && !(fields & bit);
            fields |= bit;
        }
        else if (section == 2)
        {
            if (equal(key, "kind") && (equal(value, "player") || equal(value, "hostage") || equal(value, "point")))
            {
                g_AnchorKinds[item] = equal(value, "player") ? Anchor_Player : equal(value, "hostage") ? Anchor_Hostage : Anchor_Point;
                bit = 1;
            }
            else if (equal(key, "origin") && Maps_Vector(value, g_AnchorOrigins[item])) { bit = 2; }
            else if (equal(key, "angles") && Maps_Vector(value, g_AnchorAngles[item], 360.0)) { bit = 4; }
            valid = bit != 0 && !(g_AnchorFields[item] & bit);
            g_AnchorFields[item] |= bit;
        }
        else if (section == 3)
        {
            if (equal(key, "anchors") && strlen(value) < charsmax(g_RouteReferences[]))
            {
                copy(g_RouteReferences[item], charsmax(g_RouteReferences[]), value);
                bit = 1;
            }
            else if (equal(key, "speed") && Maps_Number(value, g_RouteSpeeds[item], 1000.0) && g_RouteSpeeds[item] > 0.0) { bit = 2; }
            else if (equal(key, "arrival_radius") && Maps_Number(value, g_RouteRadii[item], 128.0) && g_RouteRadii[item] > 0.0) { bit = 4; }
            else if (equal(key, "waypoint_timeout") && Maps_Number(value, g_RouteTimeouts[item], 120.0) && g_RouteTimeouts[item] > 0.0) { bit = 8; }
            valid = bit != 0 && !(g_RouteFields[item] & bit);
            g_RouteFields[item] |= bit;
        }
        else if (section == 4)
        {
            if (equal(key, "viewer") && Maps_IsName(value))
            {
                copy(g_ViewReferences[item][0], charsmax(g_ViewReferences[][]), value);
                bit = 1;
            }
            else if (equal(key, "target") && Maps_IsName(value))
            {
                copy(g_ViewReferences[item][1], charsmax(g_ViewReferences[][]), value);
                bit = 2;
            }
            else if (equal(key, "los") && (equal(value, "clear") || equal(value, "blocked")))
            {
                g_ViewLos[item] = bool:equal(value, "clear");
                bit = 4;
            }
            else if (equal(key, "pvs") && (equal(value, "inside") || equal(value, "outside")))
            {
                g_ViewPvs[item] = bool:equal(value, "inside");
                bit = 8;
            }
            valid = bit != 0 && !(g_ViewFields[item] & bit);
            g_ViewFields[item] |= bit;
        }
        else
        {
            valid = false;
        }
    }
    fclose(file);
    formatex(path, charsmax(path), "maps/%s.bsp", map_name);
    hash_file(path, Hash_Sha256, g_MapHash, charsmax(g_MapHash));
    if (!valid || fields != 15 || !equal(expected_hash, g_MapHash))
    {
        return false;
    }
    for (new index = 0; index < g_AnchorCount; index++)
    {
        if (g_AnchorFields[index] != 7) { return false; }
    }
    for (new index = 0; index < g_RouteCount; index++)
    {
        if (g_RouteFields[index] != 15) { return false; }
        new cursor, token[32];
        while (g_RouteReferences[index][cursor])
        {
            while (g_RouteReferences[index][cursor] == ' ' || g_RouteReferences[index][cursor] == '^t') { cursor++; }
            new length;
            while (g_RouteReferences[index][cursor] && g_RouteReferences[index][cursor] != ' ' && g_RouteReferences[index][cursor] != '^t')
            {
                if (length == charsmax(token)) { return false; }
                token[length++] = g_RouteReferences[index][cursor++];
            }
            token[length] = 0;
            if (!length) { break; }
            new anchor = Maps_FindAnchor(token);
            if (anchor < 0 || g_AnchorKinds[anchor] != Anchor_Player || g_RouteLengths[index] == sizeof g_RouteAnchors[]) { return false; }
            g_RouteAnchors[index][g_RouteLengths[index]++] = anchor;
        }
        if (!g_RouteLengths[index]) { return false; }
    }
    for (new index = 0; index < g_ViewCount; index++)
    {
        if (g_ViewFields[index] != 15) { return false; }
        for (new side = 0; side < 2; side++)
        {
            g_ViewAnchors[index][side] = Maps_FindAnchor(g_ViewReferences[index][side]);
            if (g_ViewAnchors[index][side] < 0) { return false; }
        }
        if (g_AnchorKinds[g_ViewAnchors[index][0]] != Anchor_Player) { return false; }
    }
    return true;
}

bool:Maps_CheckPlayerAnchor(anchor)
{
    if (anchor < 0 || g_AnchorKinds[anchor] != Anchor_Player)
    {
        return false;
    }
    new trace = create_tr2();
    engfunc(EngFunc_TraceHull, g_AnchorOrigins[anchor], g_AnchorOrigins[anchor], IGNORE_MONSTERS, HULL_HUMAN, 0, trace);
    new bool:valid = !get_tr2(trace, TR_StartSolid) && !get_tr2(trace, TR_AllSolid);
    new Float:bottom[3];
    for (new axis = 0; axis < 3; axis++) { bottom[axis] = g_AnchorOrigins[anchor][axis]; }
    bottom[2] -= 40.0;
    engfunc(EngFunc_TraceHull, g_AnchorOrigins[anchor], bottom, IGNORE_MONSTERS, HULL_HUMAN, 0, trace);
    new Float:fraction;
    get_tr2(trace, TR_flFraction, fraction);
    valid = valid && fraction < 1.0;
    free_tr2(trace);
    return valid;
}

bool:Maps_PlacePlayer(id, const name[])
{
    new anchor = Maps_FindAnchor(name);
    if (!Maps_CheckPlayerAnchor(anchor))
    {
        return false;
    }
    new Float:zero[3];
    set_pev(id, pev_velocity, zero);
    engfunc(EngFunc_SetOrigin, id, g_AnchorOrigins[anchor]);
    set_pev(id, pev_angles, g_AnchorAngles[anchor]);
    set_pev(id, pev_v_angle, g_AnchorAngles[anchor]);
    set_pev(id, pev_fixangle, 1);
    return true;
}
