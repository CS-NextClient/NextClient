#include <amxmodx>
#include <fakemeta>
#include <json>
#include <next_client_api>

#define MAX_CLIENTS 32
#define PARAM_OUTLINE_MODE 10

enum
{
    MenuMain,
    MenuModel,
    MenuParameters,
    MenuValue,
    MenuMove
}

new const PRESET[] = "materials/test_nextclient/material_effects.mat";
new const MASK[] = "materials/test_nextclient/effects/dissolve.png";
new const TARGET_CLASS[] = "ncl_material_effects_target";
new const MODELS[][] = {"models/player/leet/leet.mdl", "models/w_ak47.mdl", "models/chick.mdl"};
new const MODEL_NAMES[][] = {"leet", "ak47", "chick"};
new const TEXTURE_COUNTS[] = {3, 1, 1};
new const PROGRAMS[][] = {"studio", "weapon_finish"};
new const OUTLINE_MODES[][] = {"occluded only", "always, full silhouette", "visible only"};
new const PARAM_COMMANDS[][] = {"dissolve", "noise", "fill", "outline", "glow", "glitch", "glitch_speed", "noise_speed", "glitch_seed", "noise_seed", "outline_mode"};
new const PARAM_NAMES[][] = {"uDissolve", "uNoise", "uOccludedAmount", "uOutlineWidth", "uGlowStrength", "uGlitchAmount", "uGlitchSpeed", "uNoiseSpeed", "uGlitchSeed", "uNoiseSeed", "uOutlineMode"};
new const Float:PARAM_MAX[] = {1.0, 1.0, 1.0, 16.0, 4.0, 1.0, 60.0, 60.0, 32767.0, 32767.0, 2.0};
new const PARAM_LABELS[][] = {
    "Dissolve", "Noise transparency", "Occluded fill", "Outline width", "Glow strength", "Geometry glitch",
    "Glitch speed", "Noise speed", "Glitch seed", "Noise seed", "Outline mode"
};
new const Float:PARAM_MENU_STEPS[] = {0.125, 0.125, 0.125, 0.5, 0.25, 0.125, 1.0, 1.0, 1.0, 1.0, 1.0};
new const Float:PARAM_COARSE_STEPS[] = {0.25, 0.25, 0.25, 2.0, 1.0, 0.25, 10.0, 10.0, 100.0, 100.0, 1.0};
new const Float:PARAM_FINE_STEPS[] = {0.00390625, 0.00390625, 0.00390625, 0.25, 0.015625, 0.00390625, 0.25, 0.25, 1.0, 1.0, 1.0};
new const STEP_NAMES[][] = {"normal", "fine", "coarse"};
new const Float:MOVE_DISTANCES[] = {64.0, 160.0, 256.0, 512.0, 1024.0, 2048.0, 4096.0, 8192.0};
new const Float:NUMBER_CHECK_VALUES[] = {
    0.0, 0.1234567, 0.0000000001, -0.0000000001,
    Float:0x00000001, Float:0x00800000, 32767.0
};
new g_Target[MAX_CLIENTS + 1];
new g_Owner[MAX_CLIENTS + 1];
new g_Model[MAX_CLIENTS + 1];
new g_Program[MAX_CLIENTS + 1];
new Float:g_Values[MAX_CLIENTS + 1][sizeof PARAM_NAMES];
new bool:g_OutsidePvs[MAX_CLIENTS + 1];
new bool:g_Bound[MAX_CLIENTS + 1];
new g_Menu[MAX_CLIENTS + 1] = {-1, ...};
new g_MenuKind[MAX_CLIENTS + 1];
new g_MenuParam[MAX_CLIENTS + 1];
new g_InputParam[MAX_CLIENTS + 1] = {-1, ...};
new g_StepMode[MAX_CLIENTS + 1];
new bool:g_ShuttingDown;

public plugin_precache()
{
    for (new index = 0; index < sizeof MODELS; index++)
    {
        precache_model(MODELS[index]);
    }
    if (!file_exists(PRESET) || !file_exists(MASK))
    {
        set_fail_state("Material effects preset or dissolve mask is missing.");
        return;
    }
    ncl_upload_file(PRESET, PRESET);
    ncl_upload_file(MASK, MASK);
}

public plugin_init()
{
    register_plugin("NextClient Material Effects Test", "1.0.0", "NextClient");
    register_concmd("ncl_fx", "CommandEffects");
    register_concmd("ncl_fx_check_numbers", "CommandCheckNumbers");
    register_clcmd("ncl_fx_menu", "CommandMenu");
    register_clcmd("say /ncl_fx", "CommandMenu");
    register_clcmd("say_team /ncl_fx", "CommandMenu");
    register_clcmd("ncl_fx_value", "CommandValue");
}

public client_disconnected(id)
{
    CloseMenu(id);
    RemoveTarget(id);
    g_StepMode[id] = 0;
}

public plugin_end()
{
    g_ShuttingDown = true;

    for (new id = 1; id <= MAX_CLIENTS; id++)
    {
        CloseMenu(id);
        RemoveTarget(id);
    }
}

bool:IsTarget(id)
{
    new entity = g_Target[id];
    if (!g_Owner[id] || !pev_valid(entity))
    {
        return false;
    }

    new classname[64];
    pev(entity, pev_classname, classname, charsmax(classname));
    return equal(classname, TARGET_CLASS) && pev(entity, pev_iuser4) == g_Owner[id];
}

RemoveTarget(id)
{
    if (IsTarget(id))
    {
        ncl_mat_clear(g_Target[id]);
        engfunc(EngFunc_RemoveEntity, g_Target[id]);
    }

    g_Target[id] = 0;
    g_Owner[id] = 0;
    g_OutsidePvs[id] = false;
    g_Bound[id] = false;
    g_InputParam[id] = -1;
}

bool:ApplyParameters(id)
{
    for (new index = 0; index < sizeof PARAM_NAMES; index++)
    {
        if (!ncl_mat_param_all(g_Target[id], PARAM_NAMES[index], g_Values[id][index]))
        {
            Notify(id, "Parameter failed: %s", PARAM_NAMES[index]);
            return false;
        }
    }

    return true;
}

bool:BindTarget(id)
{
    g_Bound[id] = false;
    ncl_mat_clear(g_Target[id]);

    new material[64];
    for (new texture = 0; texture < TEXTURE_COUNTS[g_Model[id]]; texture++)
    {
        formatex(material, charsmax(material), "%s_%s_%d", MODEL_NAMES[g_Model[id]], PROGRAMS[g_Program[id]], texture);
        if (!ncl_mat_bind(g_Target[id], PRESET, material))
        {
            ncl_mat_clear(g_Target[id]);
            Notify(id, "Binding failed: %s", material);
            return false;
        }
    }
    if (!ApplyParameters(id) || !ncl_mat_through_walls(g_Target[id], g_OutsidePvs[id]))
    {
        ncl_mat_clear(g_Target[id]);
        Notify(id, "Binding setup failed; check the module log.");
        return false;
    }

    g_Bound[id] = true;
    return true;
}

MoveTarget(id, Float:distance)
{
    new Float:origin[3], Float:view[3], Float:angles[3], Float:direction[3];
    pev(id, pev_origin, origin);
    pev(id, pev_view_ofs, view);
    pev(id, pev_v_angle, angles);
    engfunc(EngFunc_MakeVectors, angles);
    global_get(glb_v_forward, direction);
    for (new axis = 0; axis < 3; axis++)
    {
        origin[axis] += view[axis] + direction[axis] * distance;
    }
    engfunc(EngFunc_SetOrigin, g_Target[id], origin);
}

bool:SpawnTarget(id, model)
{
    RemoveTarget(id);
    new entity = engfunc(EngFunc_CreateNamedEntity, engfunc(EngFunc_AllocString, "info_target"));
    if (!pev_valid(entity))
    {
        Notify(id, "Cannot create a target entity.");
        return false;
    }
    g_Target[id] = entity;
    g_Owner[id] = get_user_userid(id);
    g_Model[id] = model;
    g_Program[id] = 0;
    g_OutsidePvs[id] = true;
    for (new index = 0; index < sizeof PARAM_NAMES; index++)
    {
        g_Values[id][index] = 0.0;
    }
    g_Values[id][2] = 0.5;
    g_Values[id][3] = 2.0;
    g_Values[id][4] = 1.0;
    g_Values[id][6] = 12.0;
    set_pev(entity, pev_classname, TARGET_CLASS);
    set_pev(entity, pev_iuser4, g_Owner[id]);
    engfunc(EngFunc_SetModel, entity, MODELS[model]);
    engfunc(EngFunc_SetSize, entity, Float:{-16.0, -16.0, -36.0}, Float:{16.0, 16.0, 36.0});
    set_pev(entity, pev_movetype, MOVETYPE_NONE);
    set_pev(entity, pev_solid, SOLID_NOT);
    set_pev(entity, pev_sequence, 0);
    set_pev(entity, pev_frame, 0.0);
    set_pev(entity, pev_framerate, 1.0);
    set_pev(entity, pev_animtime, get_gametime());
    MoveTarget(id, 160.0);
    return BindTarget(id);
}

bool:ReadAmount(const text[], Float:maximum, &Float:value)
{
    new bool:dot = false, bool:digit = false;
    for (new index = 0; text[index] != 0; index++)
    {
        if (text[index] >= '0' && text[index] <= '9')
        {
            digit = true;
        }
        else if (text[index] == '.' && !dot)
        {
            dot = true;
        }
        else if (index != 0 || (text[index] != '-' && text[index] != '+'))
        {
            return false;
        }
    }
    value = str_to_float(text);
    return digit && value >= 0.0 && value <= maximum;
}

bool:FormatParameterValue(Float:value, text[], maxlen)
{
    new JSON:number = json_init_real(value);
    if (number == Invalid_JSON)
    {
        copy(text, maxlen, "<number unavailable>");
        return false;
    }

    // Parson serialization retains significant digits and uses exponents for small floats.
    new written = json_serial_to_string(number, text, maxlen);
    json_free(number);
    if (written == 0)
    {
        copy(text, maxlen, "<number unavailable>");
        return false;
    }

    return true;
}

public CommandCheckNumbers(id)
{
    new text[32], failures;

    for (new index = 0; index < sizeof NUMBER_CHECK_VALUES; index++)
    {
        new bool:formatted = FormatParameterValue(NUMBER_CHECK_VALUES[index], text, charsmax(text));
        new Float:restored = str_to_float(text);
        new bool:passed = formatted && _:restored == _:NUMBER_CHECK_VALUES[index];
        if (!passed)
        {
            failures++;
        }

        console_print(id, "[NCL FX] number[%d] bits=%08x text=%s restored=%08x %s",
            index, _:NUMBER_CHECK_VALUES[index], text, _:restored, passed ? "PASS" : "FAIL");
    }

    console_print(id, "[NCL FX] number formatting: %s (%d/%d)",
        failures == 0 ? "PASS" : "FAIL", sizeof NUMBER_CHECK_VALUES - failures, sizeof NUMBER_CHECK_VALUES);
    return PLUGIN_HANDLED;
}

ShowStatus(id)
{
    if (!IsTarget(id))
    {
        Notify(id, "API ready=%d; no target. Open ncl_fx_menu to create one.", ncl_is_client_api_ready(id));
        return;
    }

    client_print(id, print_console,
        "[NCL FX] entity=%d model=%s program=%s bound=%d outside_pvs=%d ready=%d",
        g_Target[id], MODEL_NAMES[g_Model[id]], PROGRAMS[g_Program[id]], g_Bound[id],
        g_OutsidePvs[id], ncl_is_client_api_ready(id));
    for (new index = 0; index < sizeof PARAM_NAMES; index++)
    {
        new value[32];
        FormatParameterValue(g_Values[id][index], value, charsmax(value));
        client_print(id, print_console, "  %s = %s", PARAM_COMMANDS[index], value);
    }

    Notify(id, "Target: %s, program: %s, binding setup: %s. Full status is in the console.",
        MODEL_NAMES[g_Model[id]], PROGRAMS[g_Program[id]], g_Bound[id] ? "OK" : "failed");
}

public CommandEffects(id)
{
    if (id <= 0 || !is_user_connected(id))
    {
        server_print("[NCL FX] Run ncl_fx from a connected player's console.");
        return PLUGIN_HANDLED;
    }
    if (read_argc() == 1)
    {
        return CommandMenu(id);
    }

    CloseMenu(id);
    new command[32], arg[64], text[64];
    read_argv(1, command, charsmax(command));
    read_argv(2, arg, charsmax(arg));
    read_argv(3, text, charsmax(text));
    if (equal(command, "remove"))
    {
        RemoveTarget(id);
        return PLUGIN_HANDLED;
    }
    if (equal(command, "status"))
    {
        ShowStatus(id);
        return PLUGIN_HANDLED;
    }
    if (equal(command, "spawn"))
    {
        if (!ncl_is_client_api_ready(id))
        {
            client_print(id, print_console, "[NCL FX] Wait for the NextClient API to become ready.");
            return PLUGIN_HANDLED;
        }
        new model = 0;
        if (arg[0] != 0)
        {
            for (model = 0; model < sizeof MODEL_NAMES; model++)
            {
                if (equal(arg, MODEL_NAMES[model]))
                {
                    break;
                }
            }
        }
        if (model >= sizeof MODEL_NAMES)
        {
            client_print(id, print_console, "[NCL FX] Models: leet, ak47, chick.");
        }
        else if (SpawnTarget(id, model))
        {
            ShowStatus(id);
        }
        return PLUGIN_HANDLED;
    }
    if (!EnsureTarget(id))
    {
        return PLUGIN_HANDLED;
    }
    if (equal(command, "program"))
    {
        if (equal(arg, "studio") || equal(arg, "weapon_finish"))
        {
            g_Program[id] = equal(arg, "weapon_finish");
            BindTarget(id);
        }
    }
    else if (equal(command, "pvs") && (equal(arg, "0") || equal(arg, "1")))
    {
        SetOutsidePvs(id, bool:equal(arg, "1"));
    }
    else if (equal(command, "move"))
    {
        new Float:distance = 160.0;
        if (arg[0] == 0 || (ReadAmount(arg, 8192.0, distance) && distance >= 16.0))
        {
            MoveTarget(id, distance);
        }
    }
    else if (equal(command, "set"))
    {
        for (new index = 0; index < sizeof PARAM_NAMES; index++)
        {
            if (equal(arg, PARAM_COMMANDS[index]))
            {
                new Float:value;
                if (ReadParameter(text, index, value))
                {
                    SetParameter(id, index, value);
                }
                else
                {
                    Notify(id, "Expected a number in 0..%.3f. Outline mode accepts only 0, 1 or 2.", PARAM_MAX[index]);
                }
                ShowStatus(id);
                return PLUGIN_HANDLED;
            }
        }
    }
    else if (!equal(command, "status"))
    {
        client_print(id, print_console, "[NCL FX] Commands: spawn, remove, move [distance], program <studio|weapon_finish>, pvs <0|1>, set <parameter> <value>, status");
    }
    ShowStatus(id);
    return PLUGIN_HANDLED;
}

Notify(id, const format[], any:...)
{
    new message[192];
    vformat(message, charsmax(message), format, 3);
    client_print(id, print_chat, "[NCL FX] %s", message);
    client_print(id, print_console, "[NCL FX] %s", message);
}

bool:EnsureTarget(id)
{
    if (!IsTarget(id))
    {
        Notify(id, "Create a target first: use the menu or ncl_fx spawn [leet|ak47|chick].");
        return false;
    }
    if (!ncl_is_client_api_ready(id))
    {
        Notify(id, "Wait for the NextClient API to become ready.");
        return false;
    }

    return true;
}

bool:SetParameter(id, parameter, Float:value)
{
    if (!EnsureTarget(id))
    {
        return false;
    }
    if (!ncl_mat_param_all(g_Target[id], PARAM_NAMES[parameter], value))
    {
        Notify(id, "Parameter update failed: %s", PARAM_NAMES[parameter]);
        return false;
    }

    g_Values[id][parameter] = value;
    return true;
}

bool:ReadParameter(const text[], parameter, &Float:value)
{
    if (!ReadAmount(text, PARAM_MAX[parameter], value))
    {
        return false;
    }
    if (parameter == PARAM_OUTLINE_MODE && value != float(floatround(value)))
    {
        return false;
    }

    return true;
}

SetOutsidePvs(id, bool:enabled)
{
    if (ncl_mat_through_walls(g_Target[id], enabled))
    {
        g_OutsidePvs[id] = enabled;
    }
    else
    {
        Notify(id, "Transmission update failed; rebind the material program.");
    }
}

CloseMenu(id)
{
    g_InputParam[id] = -1;
    if (g_Menu[id] == -1)
    {
        return;
    }

    new menu = g_Menu[id];
    // menu_destroy can invoke the cancellation handler before it returns.
    g_Menu[id] = -1;
    menu_destroy(menu);
}

Float:GetMenuStep(id, parameter)
{
    if (g_StepMode[id] == 1)
    {
        return PARAM_FINE_STEPS[parameter];
    }
    if (g_StepMode[id] == 2)
    {
        return PARAM_COARSE_STEPS[parameter];
    }

    return PARAM_MENU_STEPS[parameter];
}

ShowEffectsMenu(id, kind = MenuMain, parameter = 0)
{
    CloseMenu(id);

    new title[192], item[96];
    new menu;
    switch (kind)
    {
        case MenuMain:
        {
            new bool:target = IsTarget(id);
            formatex(title, charsmax(title), "Material effects^nAPI: %s | Target: %s%s",
                ncl_is_client_api_ready(id) ? "ready" : "waiting",
                target ? MODEL_NAMES[g_Model[id]] : "none",
                target ? (g_Bound[id] ? " (bound)" : " (binding failed)") : "");
            menu = menu_create(title, "MenuEffects");
            menu_additem(menu, "Create / replace target");
            formatex(item, charsmax(item), "Material program: %s", PROGRAMS[g_Program[id]]);
            menu_additem(menu, item);
            formatex(item, charsmax(item), "Outline: %s", OUTLINE_MODES[floatround(g_Values[id][PARAM_OUTLINE_MODE])]);
            menu_additem(menu, item);
            formatex(item, charsmax(item), "Outside-PVS transmission: %s", g_OutsidePvs[id] ? "ON" : "OFF");
            menu_additem(menu, item);
            menu_additem(menu, "Edit effect parameters");
            menu_additem(menu, "Move target along view");
            menu_additem(menu, "Print status / refresh readiness");
            menu_additem(menu, "Remove target");
        }
        case MenuModel:
        {
            menu = menu_create("Create / replace target^nResets effects; places the model 160 units ahead", "MenuEffects");
            for (new index = 0; index < sizeof MODEL_NAMES; index++)
            {
                menu_additem(menu, MODEL_NAMES[index]);
            }
            menu_additem(menu, "Back to main menu");
        }
        case MenuParameters:
        {
            menu = menu_create("Effect parameters", "MenuEffects");
            for (new index = 0; index < sizeof PARAM_NAMES; index++)
            {
                new value[32];
                FormatParameterValue(g_Values[id][index], value, charsmax(value));
                formatex(item, charsmax(item), "%s: %s", PARAM_LABELS[index], value);
                menu_additem(menu, item);
            }
            menu_additem(menu, "Back to main menu");
        }
        case MenuValue:
        {
            new Float:step = GetMenuStep(id, parameter);
            new value[32];
            FormatParameterValue(g_Values[id][parameter], value, charsmax(value));
            formatex(title, charsmax(title), "%s: %s^nRange: 0..%.3f | Step: %.3f (%s)",
                PARAM_LABELS[parameter], value,
                PARAM_MAX[parameter], step, STEP_NAMES[g_StepMode[id]]);
            menu = menu_create(title, "MenuEffects");
            formatex(item, charsmax(item), "Decrease by %.3f", step);
            menu_additem(menu, item);
            formatex(item, charsmax(item), "Increase by %.3f", step);
            menu_additem(menu, item);
            menu_additem(menu, "Cycle step: normal / fine / coarse");
            menu_additem(menu, "Set to minimum (0)");
            menu_additem(menu, "Set to maximum");
            menu_additem(menu, "Enter a number");
            menu_additem(menu, "Back to parameters");
        }
        case MenuMove:
        {
            menu = menu_create("Move target along current view^nDistance is measured from the camera", "MenuEffects");
            for (new index = 0; index < sizeof MOVE_DISTANCES; index++)
            {
                formatex(item, charsmax(item), "Place %.0f units ahead", MOVE_DISTANCES[index]);
                menu_additem(menu, item);
            }
            menu_additem(menu, "Back to main menu");
        }
    }
    if (kind != MenuParameters)
    {
        menu_setprop(menu, MPROP_PERPAGE, 0);
        menu_setprop(menu, MPROP_EXIT, MEXIT_FORCE);
    }

    g_Menu[id] = menu;
    g_MenuKind[id] = kind;
    g_MenuParam[id] = parameter;
    menu_display(id, menu, kind == MenuParameters ? parameter / 7 : 0);
}

public CommandMenu(id)
{
    if (id > 0 && is_user_connected(id) && !g_ShuttingDown)
    {
        ShowEffectsMenu(id);
    }

    return PLUGIN_HANDLED;
}

public MenuEffects(id, menu, item)
{
    if (g_Menu[id] != menu)
    {
        return PLUGIN_HANDLED;
    }

    new kind = g_MenuKind[id];
    new parameter = g_MenuParam[id];
    g_Menu[id] = -1;
    menu_destroy(menu);

    // Cancel and timeout callbacks must not display another menu.
    if (item < 0 || g_ShuttingDown || !is_user_connected(id))
    {
        return PLUGIN_HANDLED;
    }

    if (kind == MenuMain && item == 0)
    {
        ShowEffectsMenu(id, MenuModel);
        return PLUGIN_HANDLED;
    }
    if (kind == MenuMain && item == 6)
    {
        ShowStatus(id);
    }
    else if (kind == MenuMain && item == 7)
    {
        RemoveTarget(id);
        Notify(id, "Target removed.");
    }
    else if (kind == MenuModel)
    {
        if (item < sizeof MODEL_NAMES)
        {
            if (!ncl_is_client_api_ready(id))
            {
                Notify(id, "Wait for the NextClient API to become ready.");
            }
            else if (SpawnTarget(id, item))
            {
                Notify(id, "Target created: %s.", MODEL_NAMES[g_Model[id]]);
            }
        }
    }
    else if ((kind == MenuParameters && item == sizeof PARAM_NAMES)
        || (kind == MenuMove && item == sizeof MOVE_DISTANCES))
    {
        ShowEffectsMenu(id);
        return PLUGIN_HANDLED;
    }
    else if (EnsureTarget(id))
    {
        switch (kind)
        {
            case MenuMain:
            {
                switch (item)
                {
                    case 1:
                    {
                        g_Program[id] = (g_Program[id] + 1) % sizeof PROGRAMS;
                        BindTarget(id);
                    }
                    case 2:
                    {
                        SetParameter(id, PARAM_OUTLINE_MODE, float((floatround(g_Values[id][PARAM_OUTLINE_MODE]) + 1) % sizeof OUTLINE_MODES));
                    }
                    case 3:
                    {
                        SetOutsidePvs(id, !g_OutsidePvs[id]);
                    }
                    case 4:
                    {
                        ShowEffectsMenu(id, MenuParameters);
                        return PLUGIN_HANDLED;
                    }
                    case 5:
                    {
                        ShowEffectsMenu(id, MenuMove);
                        return PLUGIN_HANDLED;
                    }
                }
            }
            case MenuParameters:
            {
                ShowEffectsMenu(id, MenuValue, item);
                return PLUGIN_HANDLED;
            }
            case MenuMove:
            {
                MoveTarget(id, MOVE_DISTANCES[item]);
                ShowEffectsMenu(id, MenuMove);
                return PLUGIN_HANDLED;
            }
            case MenuValue:
            {
                if (item == 6)
                {
                    ShowEffectsMenu(id, MenuParameters, parameter);
                    return PLUGIN_HANDLED;
                }
                if (item == 5)
                {
                    g_InputParam[id] = parameter;
                    Notify(id, "Enter %s in 0..%.3f, or cancel. Ordinary scalar input retains float precision.",
                        PARAM_COMMANDS[parameter], PARAM_MAX[parameter]);
                    client_cmd(id, "messagemode ncl_fx_value");
                    return PLUGIN_HANDLED;
                }
                if (item == 2)
                {
                    g_StepMode[id] = (g_StepMode[id] + 1) % sizeof STEP_NAMES;
                }
                else
                {
                    new Float:maximum = PARAM_MAX[parameter];
                    new Float:value = g_Values[id][parameter];
                    switch (item)
                    {
                        case 0:
                        {
                            value -= GetMenuStep(id, parameter);
                        }
                        case 1:
                        {
                            value += GetMenuStep(id, parameter);
                        }
                        case 3:
                        {
                            value = 0.0;
                        }
                        case 4:
                        {
                            value = maximum;
                        }
                    }
                    SetParameter(id, parameter, floatclamp(value, 0.0, maximum));
                }
                ShowEffectsMenu(id, MenuValue, parameter);
                return PLUGIN_HANDLED;
            }
        }
    }

    ShowEffectsMenu(id);
    return PLUGIN_HANDLED;
}

public CommandValue(id)
{
    if (id <= 0 || !is_user_connected(id) || g_ShuttingDown)
    {
        return PLUGIN_HANDLED;
    }

    new parameter = g_InputParam[id];
    g_InputParam[id] = -1;
    if (parameter < 0)
    {
        Notify(id, "Choose Enter a number in the effect menu first.");
        return PLUGIN_HANDLED;
    }
    if (!EnsureTarget(id))
    {
        ShowEffectsMenu(id);
        return PLUGIN_HANDLED;
    }

    new text[64];
    read_args(text, charsmax(text));
    remove_quotes(text);
    trim(text);
    if (!equali(text, "cancel"))
    {
        new Float:value;
        if (ReadParameter(text, parameter, value))
        {
            SetParameter(id, parameter, value);
        }
        else
        {
            Notify(id, "Expected a number in 0..%.3f. Outline mode accepts only 0, 1 or 2.", PARAM_MAX[parameter]);
        }
    }

    ShowEffectsMenu(id, MenuValue, parameter);
    return PLUGIN_HANDLED;
}
