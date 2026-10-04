#include "plugin_bridge.h"
#include "plugin_game_state.h"
#include "color_chat_in_console.h"
#include <net_api.h>
#include <array>
#include <map>
#include "main.h"
#include "plugin_movement.h"
#include <nextclient/runtime.h>
#include <triangleapi.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace
{
    bool prediction_ready{};
    bool bridge_ready{};
    PluginGameState game_state;
    pfnEngSrc_pfnHookUserMsg_t original_hook{};
    std::map<std::string, pfnUserMsgHook> message_handlers;
    std::set<std::string> watched_messages;
    std::shared_ptr<nitroapi::Unsubscriber> cvar_hook, state_hook, disconnect_hook;
    struct CvarStorage
    {
        std::string initial;
        int32_t archive;
    };
    std::map<std::string, CvarStorage> cvar_storage;
    std::string connection_identity, current_map;
    int server_epoch = -1;
    std::array<int, 33> player_ids{};
    using Json = tao::json::value;
    void Emit(const char* name, const Json& data)
    {
        try
        {
            const auto json = tao::json::to_string(data);
            nc_runtime_event(name, json.c_str());
        }
        catch (...)
        {}
    }
    void SyncEpoch()
    {
        const auto* state = eng()->client_state;
        const auto* cls = eng()->client_static;
        const int epoch = cls && cls->state >= ca_connected && state ? state->servercount : -1;
        if (epoch != server_epoch)
        {
            game_state.Reset();
            player_ids.fill(0);
            server_epoch = epoch;
            prediction_ready = false;
        }
    }
    void ClearGameState()
    {
        game_state.Reset();
        player_ids.fill(0);
        server_epoch = -1;
        prediction_ready = false;
    }
    void SyncPlayers();
    int DispatchMessage(const char* name, int size, void* bytes)
    {
        auto handler = message_handlers.find(name ? name : "");
        if (handler == message_handlers.end())
            return 0;
        SyncEpoch();
        if (bridge_ready)
            SyncPlayers();
        uint8_t replacement[4096]{};
        uint32_t replacement_size = sizeof(replacement);
        const int action = bridge_ready && size >= 0 ? nc_runtime_message(
                                                           name,
                                                           static_cast<const uint8_t*>(bytes),
                                                           static_cast<uint32_t>(size),
                                                           gEngfuncs.GetClientTime(),
                                                           server_epoch,
                                                           replacement,
                                                           &replacement_size
                                                       )
                                                     : 0;
        // Parse canonical data before the presentation consumer can mutate it.
        if (bridge_ready && size >= 0)
            for (const auto& event : game_state.Message(name, bytes, static_cast<size_t>(size), gEngfuncs.GetClientTime()))
                Emit(event.name.c_str(), event.data);
        return action == 2 || !handler->second
                   ? 1
                   : handler->second(name, action == 1 ? static_cast<int>(replacement_size) : size, action == 1 ? replacement : bytes);
    }
    int HookMessage(const char* name, pfnUserMsgHook handler)
    {
        if (name && handler && handler != DispatchMessage)
        {
            message_handlers[name] = handler;
            if (PluginGameState::Observes(name) || watched_messages.count(name))
                return original_hook(name, DispatchMessage);
        }
        return original_hook(name, handler);
    }
    int32_t WatchMessage(const char* name)
    {
        if (!original_hook)
            return 0;
        watched_messages.insert(name);
        message_handlers.try_emplace(name, nullptr);
        return original_hook(name, DispatchMessage) != 0;
    }
    float frame_time{};
    // Engine command registrations keep their names; preserve storage until DLL shutdown.
    std::set<std::string> command_names;
    void DispatchCommand()
    {
        const int count = gEngfuncs.Cmd_Argc();
        if (count < 1 || count > 64)
            return;
        std::vector<const char*> args;
        for (int i = 0; i < count; ++i)
            args.push_back(gEngfuncs.Cmd_Argv(i));
        nc_runtime_console(count, args.data());
    }
    int32_t RegisterCommand(const char* name)
    {
        for (auto handle = gEngfuncs.GetFirstCmdFunctionHandle(); handle; handle = gEngfuncs.GetNextCmdFunctionHandle(handle))
            if (!_stricmp(name, gEngfuncs.GetCmdFunctionName(handle)))
                return command_names.count(name) != 0;
        if (gEngfuncs.pfnGetCvarPointer(name))
            return 0;
        const auto& stored = *command_names.insert(name).first;
        return gEngfuncs.pfnAddCommand(const_cast<char*>(stored.c_str()), DispatchCommand) != 0;
    }
    bool InGame()
    {
        auto* local = gEngfuncs.GetLocalPlayer();
        return eng()->client_static && eng()->client_static->state == ca_active && prediction_ready && local && local->model && pmove;
    }
    void CopyVector(float (&target)[3], const float* source)
    {
        std::copy_n(source, 3, target);
    }
    int32_t GetPlayer(NcPlayerState* value)
    {
        *value = {};
        value->size = sizeof(*value);
        if (!InGame())
            return 0;
        value->flags = PluginMovementFlags(pmove);
        if (!pmove->spectator && !pmove->iuser1)
            value->flags |= NC_PLAYER_ACTIVE;
        value->index = gEngfuncs.GetLocalPlayer()->index;
        value->health = gHUD ? gHUD->m_Health->m_iHealth : static_cast<int32_t>(g_LastPlayerState.client.health);
        // Nitro does not bind the optional battery HUD. Use the received
        // Battery message instead of dereferencing its null HUD pointer.
        value->armor = game_state.Armor();
        value->weapon_id = g_LastPlayerState.client.m_iId;
        value->weapons = static_cast<uint32_t>(gHUD ? *gHUD->m_iWeaponBits : g_LastPlayerState.client.weapons);
        CopyVector(value->position, pmove->origin);
        CopyVector(value->velocity, pmove->velocity);
        gEngfuncs.GetViewAngles(value->view_angles);
        CopyVector(value->view_offset, pmove->view_ofs);
        value->fov = g_LastPlayerState.client.fov;
        value->max_speed = pmove->maxspeed;
        value->water_level = pmove->waterlevel;
        value->move_type = pmove->movetype;
        return 1;
    }
    int32_t GetEntity(int32_t index, NcEntity* value)
    {
        *value = {};
        value->size = sizeof(*value);
        if (!InGame() || index < 1 || index > 8191)
            return 0;
        auto* entity = gEngfuncs.GetEntityByIndex(index);
        auto* local = gEngfuncs.GetLocalPlayer();
        // Reject stale entities retained from previous network frames/maps.
        if (!entity || !entity->model || entity->curstate.messagenum != local->curstate.messagenum)
            return 0;
        const auto& state = entity->curstate;
        value->index = index;
        value->flags = entity->player ? NC_ENTITY_PLAYER : 0;
        if (index == local->index)
        {
            value->flags |= NC_ENTITY_LOCAL | NC_ENTITY_HEALTH;
            value->health = gHUD ? gHUD->m_Health->m_iHealth : static_cast<int32_t>(g_LastPlayerState.client.health);
        }
        // Remote health/weapon inventory is not reliably transmitted by CS.
        value->model_index = state.modelindex;
        value->owner = state.owner;
        value->team = state.team;
        CopyVector(value->position, entity->origin);
        CopyVector(value->angles, entity->angles);
        CopyVector(value->velocity, state.velocity);
        CopyVector(value->mins, state.mins);
        CopyVector(value->maxs, state.maxs);
        value->sequence = state.sequence;
        value->effects = state.effects;
        value->move_type = state.movetype;
        return 1;
    }
    int32_t GetWeapon(int32_t id, NcWeapon* value)
    {
        *value = {};
        value->size = sizeof(*value);
        if (!InGame() || id < 1 || id >= 64)
            return 0;
        const auto& weapon = g_LastPlayerState.weapondata[id];
        if (weapon.m_iId != id)
            return 0;
        value->id = id;
        const auto weapons = static_cast<uint32_t>(gHUD ? *gHUD->m_iWeaponBits : g_LastPlayerState.client.weapons);
        value->owned = id < 32 && (weapons & (1u << id)) != 0;
        value->clip = weapon.m_iClip;
        value->reloading = weapon.m_fInReload;
        value->state = weapon.m_iWeaponState;
        value->next_primary_attack = weapon.m_flNextPrimaryAttack;
        value->next_secondary_attack = weapon.m_flNextSecondaryAttack;
        value->idle_time = weapon.m_flTimeWeaponIdle;
        return 1;
    }
    uint32_t ReadCvar(const char* name, char* buffer, uint32_t capacity)
    {
        auto* cvar = gEngfuncs.pfnGetCvarPointer(name);
        if (!cvar || !cvar->string)
            return 0;
        const auto length = strnlen_s(cvar->string, 65536);
        if (length >= 65536)
            return 0;
        const auto required = static_cast<uint32_t>(length + 1);
        if (buffer && capacity >= required)
            std::memcpy(buffer, cvar->string, required);
        return required;
    }
    int32_t WriteCvar(const char* name, const char* value)
    {
        if (!gEngfuncs.pfnGetCvarPointer(name))
            return 0;
        // Direct cvar API: never interpolate values into executable console text.
        eng()->Cvar_Set(name, value);
        return 1;
    }
    int32_t CreateCvar(const char* name, const char* initial, int32_t archive)
    {
        if (gEngfuncs.pfnGetCvarPointer(name))
            return cvar_storage.count(name) != 0;
        for (auto handle = gEngfuncs.GetFirstCmdFunctionHandle(); handle; handle = gEngfuncs.GetNextCmdFunctionHandle(handle))
            if (!_stricmp(name, gEngfuncs.GetCmdFunctionName(handle)))
                return 0;
        const auto it = cvar_storage.try_emplace(name, CvarStorage{initial, archive}).first;
        return gEngfuncs.pfnRegisterVariable(it->first.c_str(), it->second.initial.c_str(), archive ? FCVAR_ARCHIVE : 0) != nullptr;
    }
    int32_t SendChat(const char* text, int32_t team)
    {
        if (!eng()->client_static || eng()->client_static->state != ca_active || eng()->client_static->demoplayback)
            return 0;
        const auto command = std::string(team ? "say_team \"" : "say \"") + text + "\"\n";
        return gEngfuncs.pfnServerCmd(command.c_str()) != 0;
    }
    int32_t ChatPrint(const char* text)
    {
        if (!bridge_ready || !eng()->client_static || eng()->client_static->state != ca_active)
            return 0;
        auto handler = message_handlers.find("SayText");
        // Invoke the original local handler directly. This neither sends chat
        // to the server nor re-emits synthetic messages to chat subscribers.
        return handler != message_handlers.end() && PrintLocalChat(handler->second, text) != 0;
    }
    int32_t Connect(const char* host, uint32_t port)
    {
        const auto command = std::string("connect ") + host + ":" + std::to_string(port) + "\n";
        return gEngfuncs.pfnClientCmd(command.c_str()) != 0;
    }
    int32_t Disconnect()
    {
        return gEngfuncs.pfnClientCmd("disconnect\n") != 0;
    }
    void DrawRect(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t rgba)
    {
        gEngfuncs.pfnFillRGBA(x, y, width, height, (rgba >> 24) & 255, (rgba >> 16) & 255, (rgba >> 8) & 255, rgba & 255);
    }
    template <size_t N>
    void CopyText(char (&target)[N], const char* source)
    {
        if (source)
            strncpy_s(target, source, _TRUNCATE);
    }
    int32_t GetSession(NcSession* value)
    {
        *value = {};
        value->size = sizeof(*value);
        SCREENINFO screen{sizeof(SCREENINFO)};
        gEngfuncs.pfnGetScreenInfo(&screen);
        value->width = screen.iWidth;
        value->height = screen.iHeight;
        value->frame_time = frame_time;
        const auto* client = eng()->client_static;
        if (client && client->state >= ca_connected)
        {
            value->flags |= NC_SESSION_CONNECTED;
            value->max_clients = gEngfuncs.GetMaxClients();
            value->time = gEngfuncs.GetClientTime();
            CopyText(value->map, gEngfuncs.pfnGetLevelName());
        }
        if (InGame())
            value->flags |= NC_SESSION_IN_GAME;
        return 1;
    }
    int32_t GetPlayerInfo(int32_t index, NcPlayerInfo* value)
    {
        *value = {};
        value->size = sizeof(*value);
        if (!InGame() || index < 1 || index > gEngfuncs.GetMaxClients())
            return 0;
        hud_player_info_t info{};
        gEngfuncs.pfnGetPlayerInfo(index, &info);
        if (!info.name || !*info.name)
            return 0;
        value->index = index;
        value->ping = info.ping;
        value->packet_loss = info.packetloss;
        value->local = info.thisplayer != 0;
        value->spectator = info.spectator != 0;
        CopyText(value->name, info.name);
        CopyText(value->model, info.model);
        return 1;
    }
    int32_t WorldToScreen(const float* world, float* screen)
    {
        if (!InGame() || !gEngfuncs.pTriAPI)
            return 0;
        float point[3]{world[0], world[1], world[2]}, projected[3]{};
        if (gEngfuncs.pTriAPI->WorldToScreen(point, projected))
            return 0;
        SCREENINFO info{sizeof(SCREENINFO)};
        if (!gEngfuncs.pfnGetScreenInfo(&info) || info.iWidth <= 0 || info.iHeight <= 0)
            return 0;
        screen[0] = (1 + projected[0]) * info.iWidth * 0.5f;
        screen[1] = (1 - projected[1]) * info.iHeight * 0.5f;
        return 1;
    }
    int32_t MeasureText(const char* text, int32_t* width, int32_t* height)
    {
        int w{}, h{};
        gEngfuncs.pfnDrawConsoleStringLen(text, &w, &h);
        *width = w;
        *height = h;
        return 1;
    }
    void DrawText(int32_t x, int32_t y, const char* text, uint32_t rgb)
    {
        gEngfuncs.pfnDrawSetTextColor(((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f);
        gEngfuncs.pfnDrawConsoleString(x, y, const_cast<char*>(text));
        gEngfuncs.pfnDrawSetTextColor(1, 1, 1);
    }
    void PlaySound(const char* path, float volume)
    {
        gEngfuncs.pfnPlaySoundByName(const_cast<char*>(path), volume);
    }

    Json VectorJson(const float* values, size_t count = 3)
    {
        Json result = tao::json::empty_array;
        for (size_t i = 0; i < count; ++i)
            result.push_back(values[i]);
        return result;
    }
    template <size_t N>
    std::string ArrayText(const char (&value)[N])
    {
        return {value, strnlen_s(value, N)};
    }
    Json EntityState(const entity_state_t& state)
    {
        Json result = tao::json::empty_object;
        result["entityType"] = state.entityType;
        result["number"] = state.number;
        result["msg_time"] = state.msg_time;
        result["messagenum"] = state.messagenum;
        result["modelindex"] = state.modelindex;
        result["sequence"] = state.sequence;
        result["frame"] = state.frame;
        result["colormap"] = state.colormap;
        result["skin"] = state.skin;
        result["solid"] = state.solid;
        result["effects"] = state.effects;
        result["scale"] = state.scale;
        result["eflags"] = state.eflags;
        result["rendermode"] = state.rendermode;
        result["renderamt"] = state.renderamt;
        result["renderfx"] = state.renderfx;
        result["movetype"] = state.movetype;
        result["animtime"] = state.animtime;
        result["framerate"] = state.framerate;
        result["body"] = state.body;
        result["aiment"] = state.aiment;
        result["owner"] = state.owner;
        result["friction"] = state.friction;
        result["gravity"] = state.gravity;
        result["team"] = state.team;
        result["playerclass"] = state.playerclass;
        result["health"] = state.health;
        result["spectator"] = state.spectator;
        result["weaponmodel"] = state.weaponmodel;
        result["gaitsequence"] = state.gaitsequence;
        result["usehull"] = state.usehull;
        result["oldbuttons"] = state.oldbuttons;
        result["onground"] = state.onground;
        result["iStepLeft"] = state.iStepLeft;
        result["flFallVelocity"] = state.flFallVelocity;
        result["fov"] = state.fov;
        result["weaponanim"] = state.weaponanim;
        result["impacttime"] = state.impacttime;
        result["starttime"] = state.starttime;
        result["iuser1"] = state.iuser1;
        result["iuser2"] = state.iuser2;
        result["iuser3"] = state.iuser3;
        result["iuser4"] = state.iuser4;
        result["fuser1"] = state.fuser1;
        result["fuser2"] = state.fuser2;
        result["fuser3"] = state.fuser3;
        result["fuser4"] = state.fuser4;
        result["origin"] = VectorJson(state.origin);
        result["angles"] = VectorJson(state.angles);
        result["velocity"] = VectorJson(state.velocity);
        result["mins"] = VectorJson(state.mins);
        result["maxs"] = VectorJson(state.maxs);
        result["basevelocity"] = VectorJson(state.basevelocity);
        result["startpos"] = VectorJson(state.startpos);
        result["endpos"] = VectorJson(state.endpos);
        result["vuser1"] = VectorJson(state.vuser1);
        result["vuser2"] = VectorJson(state.vuser2);
        result["vuser3"] = VectorJson(state.vuser3);
        result["vuser4"] = VectorJson(state.vuser4);

        result["rendercolor"] = Json::array({state.rendercolor.r, state.rendercolor.g, state.rendercolor.b});
        result["controller"] = Json::array({state.controller[0], state.controller[1], state.controller[2], state.controller[3]});
        result["blending"] = Json::array({state.blending[0], state.blending[1], state.blending[2], state.blending[3]});
        return result;
    }
    Json ClientState(const clientdata_t& state)
    {
        Json result = tao::json::empty_object;
        result["viewmodel"] = state.viewmodel;
        result["flags"] = state.flags;
        result["waterlevel"] = state.waterlevel;
        result["watertype"] = state.watertype;
        result["health"] = state.health;
        result["bInDuck"] = state.bInDuck;
        result["weapons"] = state.weapons;
        result["flTimeStepSound"] = state.flTimeStepSound;
        result["flDuckTime"] = state.flDuckTime;
        result["flSwimTime"] = state.flSwimTime;
        result["waterjumptime"] = state.waterjumptime;
        result["maxspeed"] = state.maxspeed;
        result["fov"] = state.fov;
        result["weaponanim"] = state.weaponanim;
        result["m_iId"] = state.m_iId;
        result["ammo_shells"] = state.ammo_shells;
        result["ammo_nails"] = state.ammo_nails;
        result["ammo_cells"] = state.ammo_cells;
        result["ammo_rockets"] = state.ammo_rockets;
        result["m_flNextAttack"] = state.m_flNextAttack;
        result["tfstate"] = state.tfstate;
        result["pushmsec"] = state.pushmsec;
        result["deadflag"] = state.deadflag;
        result["iuser1"] = state.iuser1;
        result["iuser2"] = state.iuser2;
        result["iuser3"] = state.iuser3;
        result["iuser4"] = state.iuser4;
        result["fuser1"] = state.fuser1;
        result["fuser2"] = state.fuser2;
        result["fuser3"] = state.fuser3;
        result["fuser4"] = state.fuser4;
        result["origin"] = VectorJson(state.origin);
        result["velocity"] = VectorJson(state.velocity);
        result["punchangle"] = VectorJson(state.punchangle);
        result["view_ofs"] = VectorJson(state.view_ofs);
        result["vuser1"] = VectorJson(state.vuser1);
        result["vuser2"] = VectorJson(state.vuser2);
        result["vuser3"] = VectorJson(state.vuser3);
        result["vuser4"] = VectorJson(state.vuser4);

        result["physinfo"] = ArrayText(state.physinfo);
        return result;
    }
    Json MovementState()
    {
        Json result = tao::json::empty_object;
        result["player_index"] = pmove->player_index;
        result["multiplayer"] = pmove->multiplayer;
        result["time"] = pmove->time;
        result["frametime"] = pmove->frametime;
        result["flDuckTime"] = pmove->flDuckTime;
        result["bInDuck"] = pmove->bInDuck;
        result["flTimeStepSound"] = pmove->flTimeStepSound;
        result["iStepLeft"] = pmove->iStepLeft;
        result["flFallVelocity"] = pmove->flFallVelocity;
        result["flSwimTime"] = pmove->flSwimTime;
        result["flNextPrimaryAttack"] = pmove->flNextPrimaryAttack;
        result["effects"] = pmove->effects;
        result["flags"] = pmove->flags;
        result["usehull"] = pmove->usehull;
        result["gravity"] = pmove->gravity;
        result["friction"] = pmove->friction;
        result["oldbuttons"] = pmove->oldbuttons;
        result["waterjumptime"] = pmove->waterjumptime;
        result["dead"] = pmove->dead;
        result["deadflag"] = pmove->deadflag;
        result["spectator"] = pmove->spectator;
        result["movetype"] = pmove->movetype;
        result["onground"] = pmove->onground;
        result["waterlevel"] = pmove->waterlevel;
        result["watertype"] = pmove->watertype;
        result["oldwaterlevel"] = pmove->oldwaterlevel;
        result["maxspeed"] = pmove->maxspeed;
        result["clientmaxspeed"] = pmove->clientmaxspeed;
        result["iuser1"] = pmove->iuser1;
        result["iuser2"] = pmove->iuser2;
        result["iuser3"] = pmove->iuser3;
        result["iuser4"] = pmove->iuser4;
        result["fuser1"] = pmove->fuser1;
        result["fuser2"] = pmove->fuser2;
        result["fuser3"] = pmove->fuser3;
        result["fuser4"] = pmove->fuser4;
        result["forward"] = VectorJson(pmove->forward);
        result["right"] = VectorJson(pmove->right);
        result["up"] = VectorJson(pmove->up);
        result["origin"] = VectorJson(pmove->origin);
        result["angles"] = VectorJson(pmove->angles);
        result["oldangles"] = VectorJson(pmove->oldangles);
        result["velocity"] = VectorJson(pmove->velocity);
        result["movedir"] = VectorJson(pmove->movedir);
        result["basevelocity"] = VectorJson(pmove->basevelocity);
        result["view_ofs"] = VectorJson(pmove->view_ofs);
        result["punchangle"] = VectorJson(pmove->punchangle);
        result["vuser1"] = VectorJson(pmove->vuser1);
        result["vuser2"] = VectorJson(pmove->vuser2);
        result["vuser3"] = VectorJson(pmove->vuser3);
        result["vuser4"] = VectorJson(pmove->vuser4);
        result["texture"] = ArrayText(pmove->sztexturename);
        result["texture_type"] = std::string(1, pmove->chtexturetype);
        return result;
    }
    Json WeaponData(int id)
    {
        if (id < 1 || id >= 64 || !InGame())
            return tao::json::null;
        auto definition = game_state.Weapon(id);
        const auto& state = g_LastPlayerState.weapondata[id];
        if (definition.is_null() && state.m_iId != id)
            return tao::json::null;
        Json result{
            {"id", id},
            {"definition", definition},
            {"prediction", tao::json::null},
            {"owned", id < 32 && (static_cast<uint32_t>(*gHUD->m_iWeaponBits) & (1u << id)) != 0},
            {"reserve", tao::json::null},
            {"reserve2", tao::json::null}
        };
        if (state.m_iId == id)
        {
            Json prediction = tao::json::empty_object;
            prediction["m_iId"] = state.m_iId;
            prediction["m_iClip"] = state.m_iClip;
            prediction["m_flNextPrimaryAttack"] = state.m_flNextPrimaryAttack;
            prediction["m_flNextSecondaryAttack"] = state.m_flNextSecondaryAttack;
            prediction["m_flTimeWeaponIdle"] = state.m_flTimeWeaponIdle;
            prediction["m_fInReload"] = state.m_fInReload;
            prediction["m_fInSpecialReload"] = state.m_fInSpecialReload;
            prediction["m_flNextReload"] = state.m_flNextReload;
            prediction["m_flPumpTime"] = state.m_flPumpTime;
            prediction["m_fReloadTime"] = state.m_fReloadTime;
            prediction["m_fAimedDamage"] = state.m_fAimedDamage;
            prediction["m_fNextAimBonus"] = state.m_fNextAimBonus;
            prediction["m_fInZoom"] = state.m_fInZoom;
            prediction["m_iWeaponState"] = state.m_iWeaponState;
            prediction["iuser1"] = state.iuser1;
            prediction["iuser2"] = state.iuser2;
            prediction["iuser3"] = state.iuser3;
            prediction["iuser4"] = state.iuser4;
            prediction["fuser1"] = state.fuser1;
            prediction["fuser2"] = state.fuser2;
            prediction["fuser3"] = state.fuser3;
            prediction["fuser4"] = state.fuser4;

            result["prediction"] = std::move(prediction);
        }
        if (!definition.is_null())
        {
            auto ammo = game_state.Ammo();
            for (const auto& [field, output] : {std::pair{"ammo_type", "reserve"}, std::pair{"ammo2_type", "reserve2"}})
            {
                const int type = definition.at(field).as<int>();
                if (type >= 0 && type < 32)
                    result[output] = ammo.at(type);
            }
        }
        return result;
    }
    Json ConnectionData()
    {
        auto* cls = eng()->client_static;
        const int state = cls ? cls->state : ca_disconnected;
        constexpr const char* states[]{"dedicated", "disconnected", "connecting", "connected", "uninitialized", "active"};
        const bool connected = state >= ca_connected;
        const char* hostname = connected ? gEngfuncs.ServerInfo_ValueForKey("hostname") : nullptr;
        Json result{
            {"state", state >= 0 && state < 6 ? states[state] : "unknown"},
            {"connected", connected},
            {"in_game", InGame()},
            {"address", cls && state >= ca_connecting ? ArrayText(cls->servername) : ""},
            {"map", connected && gEngfuncs.pfnGetLevelName() ? gEngfuncs.pfnGetLevelName() : ""},
            {"max_clients", connected ? gEngfuncs.GetMaxClients() : 0},
            {"demo_playback", cls && cls->demoplayback != 0},
            {"demo_recording", cls && cls->demorecording != 0},
            {"hostname", hostname ? hostname : ""},
            {"latency", tao::json::null},
            {"packet_loss", tao::json::null},
            {"connection_time", tao::json::null}
        };
        if (connected && gEngfuncs.pNetAPI)
        {
            net_status_t status{};
            gEngfuncs.pNetAPI->Status(&status);
            result["latency"] = status.latency;
            result["packet_loss"] = status.packet_loss;
            result["connection_time"] = status.connection_time;
            const char* remote = gEngfuncs.pNetAPI->AdrToString(&status.remote_address);
            result["remote_address"] = remote ? remote : "";
        }
        return result;
    }
    Json ScoreboardPlayer(int index)
    {
        if (index < 1 || index > std::min(gEngfuncs.GetMaxClients(), 32))
            return tao::json::null;
        hud_player_info_t info{};
        gEngfuncs.pfnGetPlayerInfo(index, &info);
        if (!info.name || !*info.name)
            return tao::json::null;
        auto result = game_state.Player(index);
        for (const auto* key :
             {"frags",
              "deaths",
              "team",
              "team_id",
              "class",
              "dead",
              "has_c4",
              "vip",
              "location",
              "reported_health",
              "radar_position",
              "attribute_flags",
              "has_defuser",
              "reported_money"})
            if (!result.find(key))
                result[key] = tao::json::null;
        result["index"] = index;
        result["name"] = std::string(info.name);
        result["model"] = info.model ? info.model : "";
        result["ping"] = info.ping;
        result["packet_loss"] = info.packetloss;
        result["local"] = info.thisplayer != 0;
        result["spectator"] = info.spectator != 0;
        result["steam_id"] = std::to_string(info.m_nSteamID);
        if (eng()->client_state)
            result["user_id"] = eng()->client_state->players[index - 1].userid;
        return result;
    }
    void SyncPlayers()
    {
        if (!eng()->client_static || eng()->client_static->state < ca_connected || !eng()->client_state)
            return;
        for (int i = 1; i <= 32; ++i)
        {
            hud_player_info_t info{};
            if (i <= gEngfuncs.GetMaxClients())
                gEngfuncs.pfnGetPlayerInfo(i, &info);
            const int id = info.name && *info.name ? eng()->client_state->players[i - 1].userid : 0;
            if (id == player_ids[i])
                continue;
            const int previous = player_ids[i];
            player_ids[i] = id;
            game_state.RemovePlayer(i);
            if (previous)
                Emit("player.left", Json{{"index", i}, {"user_id", previous}});
            if (id)
                Emit("player.joined", ScoreboardPlayer(i));
        }
    }
    void ConnectionEvents()
    {
        const auto connection = ConnectionData();
        const auto identity =
            tao::json::to_string(Json::array({connection.at("state"), connection.at("address"), connection.at("demo_playback")}));
        if (identity != connection_identity)
        {
            connection_identity = identity;
            Emit("connection.changed", connection);
        }
        const auto map = connection.at("map").get_string();
        if (map != current_map)
        {
            Emit("map.changed", Json{{"old", current_map}, {"map", map}});
            current_map = map;
        }
        SyncPlayers();
    }
    Json GameDataValue(std::string_view section, int index)
    {
        if (section == "connection")
            return ConnectionData();
        if (!InGame())
            return tao::json::null;
        if (section == "player")
        {
            NcPlayerState player{};
            GetPlayer(&player);
            return Json{
                {"index", player.index},
                {"flags", player.flags},
                {"health", player.health},
                {"armor", player.armor},
                {"weapon_id", player.weapon_id},
                {"weapons", player.weapons},
                {"position", VectorJson(player.position)},
                {"velocity", VectorJson(player.velocity)},
                {"view_angles", VectorJson(player.view_angles)},
                {"view_offset", VectorJson(player.view_offset)},
                {"fov", player.fov},
                {"max_speed", player.max_speed},
                {"water_level", player.water_level},
                {"move_type", player.move_type},
                {"client", ClientState(g_LastPlayerState.client)},
                {"movement", MovementState()},
                {"entity", EntityState(g_LastPlayerState.playerstate)},
                {"shots_fired", client()->g_iShotsFired ? Json(*client()->g_iShotsFired) : Json(tao::json::null)},
                {"hud", game_state.Match()}
            };
        }
        if (section == "entity")
        {
            NcEntity entity{};
            if (!GetEntity(index, &entity))
                return tao::json::null;
            auto* source = gEngfuncs.GetEntityByIndex(index);
            auto result = EntityState(source->curstate);
            result["render_origin"] = VectorJson(source->origin);
            result["render_angles"] = VectorJson(source->angles);
            result["model_name"] = ArrayText(source->model->name);
            result["is_player"] = source->player != 0;
            // Network health fields remain raw; this explicitly identifies reliable local health.
            result["known_health"] = (entity.flags & NC_ENTITY_HEALTH) ? Json(entity.health) : Json(tao::json::null);
            return result;
        }
        if (section == "entities")
        {
            Json result = tao::json::empty_array;
            const int count = eng()->client_state ? std::min(eng()->client_state->max_edicts, 8192) : 8192;
            for (int i = 1; i < count; ++i)
            {
                NcEntity entity{};
                if (GetEntity(i, &entity))
                    result.push_back(i);
            }
            return result;
        }
        if (section == "weapon")
            return WeaponData(index);
        if (section == "weapons")
        {
            Json result = tao::json::empty_array;
            for (int i = 1; i < 64; ++i)
            {
                auto weapon = WeaponData(i);
                if (!weapon.is_null())
                    result.push_back(std::move(weapon));
            }
            return result;
        }
        if (section == "ammo")
            return game_state.Ammo();
        if (section == "scoreboard")
        {
            if (index)
                return ScoreboardPlayer(index);
            Json result = tao::json::empty_array;
            for (int i = 1; i <= std::min(gEngfuncs.GetMaxClients(), 32); ++i)
            {
                auto player = ScoreboardPlayer(i);
                if (!player.is_null())
                    result.push_back(std::move(player));
            }
            return result;
        }
        if (section == "match")
        {
            auto result = game_state.Match();
            result["time"] = gEngfuncs.GetClientTime();
            result["map"] = gEngfuncs.pfnGetLevelName();
            for (const auto* key : {"player.money", "round.time", "round.end", "bomb.dropped"})
                if (!result.find(key))
                    result[key] = tao::json::null;
            if (eng()->client_state)
            {
                result["paused"] = eng()->client_state->paused != 0;
                result["intermission"] = eng()->client_state->intermission != 0;
                result["view_entity"] = eng()->client_state->viewentity;
                result["game_type"] = eng()->client_state->gametype;
            }
            if (auto timer = result.find("round.time"); timer && !timer->is_null())
                result["round_seconds_remaining"] = std::max(
                    0.0,
                    timer->at("seconds").as<double>() - std::max(0.0, gEngfuncs.GetClientTime() - timer->at("received_at").as<double>())
                );
            else
                result["round_seconds_remaining"] = tao::json::null;
            return result;
        }
        return tao::json::null;
    }
    uint32_t GameData(const char* section, int32_t index, char* buffer, uint32_t capacity)
    {
        try
        {
            const auto value = GameDataValue(section, index);
            if (value.is_null())
                return 0;
            const auto json = tao::json::to_string(value);
            if (json.size() > 1024 * 1024)
                return 0;
            const auto required = static_cast<uint32_t>(json.size() + 1);
            if (buffer && capacity >= required)
                std::memcpy(buffer, json.c_str(), required);
            return required;
        }
        catch (...)
        {
            return 0;
        }
    }
    void ConsolePrint(const char* text)
    {
        PrintPluginConsole(text);
    }
} // namespace
void PluginBridgeInit()
{
    prediction_ready = false;
    bridge_ready = true;
    frame_time = 0;
    const NcClientServices services{RegisterCommand, GetPlayer,     GetEntity,     GetWeapon,   ReadCvar,   WriteCvar, DrawRect,
                                    GetSession,      GetPlayerInfo, WorldToScreen, MeasureText, DrawText,   PlaySound, ConsolePrint,
                                    GameData,        CreateCvar,    SendChat,      Connect,     Disconnect, ChatPrint, WatchMessage};
    nc_runtime_bind_client(&services);
}
void PluginBridgeShutdown()
{
    bridge_ready = false;
    prediction_ready = false;
    for (auto* hook : {&cvar_hook, &state_hook, &disconnect_hook})
    {
        if (*hook)
        {
            (*hook)->Unsubscribe();
            hook->reset();
        }
    }
    if (original_hook)
    {
        for (const auto& [name, handler] : message_handlers)
            original_hook(name.c_str(), handler);
        if (client()->gEngfuncs && client()->gEngfuncs->pfnHookUserMsg == HookMessage)
            client()->gEngfuncs->pfnHookUserMsg = original_hook;
        if (gEngfuncs.pfnHookUserMsg == HookMessage)
            gEngfuncs.pfnHookUserMsg = original_hook;
        message_handlers.clear();
        watched_messages.clear();
        original_hook = nullptr;
    }
    game_state.Reset();
    server_epoch = -1;
    current_map.clear();
    connection_identity.clear();
    player_ids.fill(0);
    nc_runtime_bind_client(nullptr);
}
void PluginBridgeReset()
{
    prediction_ready = false;
}
void PluginBridgePredictionReady()
{
    prediction_ready = true;
}
void PluginBridgeDraw(float time, int intermission)
{
    if (!InGame())
        return;
    SCREENINFO screen{sizeof(SCREENINFO)};
    gEngfuncs.pfnGetScreenInfo(&screen);
    const NcDrawContext context{sizeof(NcDrawContext), screen.iWidth, screen.iHeight, intermission, time};
    nc_runtime_draw(&context);
}
void PluginBridgeFrame(double delta)
{
    if (!bridge_ready)
        return;
    SyncEpoch();
    ConnectionEvents();
    // HUD_Frame receives host_frametime, not an absolute timestamp.
    frame_time = std::isfinite(delta) ? static_cast<float>(std::clamp(delta, 0.0, 1.0)) : 0;
    // GameUI pumps plugin callbacks once per frame, also in disconnected menus.
}

void PluginBridgePrepare()
{
    if (!original_hook && client()->gEngfuncs)
    {
        original_hook = client()->gEngfuncs->pfnHookUserMsg;
        client()->gEngfuncs->pfnHookUserMsg = HookMessage;
    }
    if (!cvar_hook)
        cvar_hook = eng()->Cvar_DirectSet |= [](cvar_t* var, const char* value, const auto& next) {
            const std::string name = var && var->name ? var->name : "";
            const std::string before = var && var->string ? var->string : "";
            next->Invoke(var, value);
            if (var && var->string)
                nc_runtime_cvar_changed(name.c_str(), before.c_str(), var->string);
        };
    // A rapid reconnect can reuse the server count without a disconnected HUD
    // frame. Clear on engine lifecycle events as well as the observed epoch.
    if (!state_hook)
        state_hook = eng()->CL_ClearState += [](qboolean) { ClearGameState(); };
    if (!disconnect_hook)
        disconnect_hook = eng()->CL_Disconnect += ClearGameState;
}
void PluginBridgeWrapMessages()
{
    if (!original_hook)
        original_hook = gEngfuncs.pfnHookUserMsg;
    if (!original_hook)
        return;
    gEngfuncs.pfnHookUserMsg = HookMessage;
    // Observe optional scoreboard messages even without a HUD consumer.
    // Chain any handlers registered afterward.
    for (const auto* name : {"HealthInfo", "Account"})
        if (message_handlers.emplace(name, nullptr).second)
            original_hook(name, DispatchMessage);
}
void PluginBridgeVoice(int index, int talking)
{
    if (bridge_ready && index >= -1 && index <= 32)
        Emit("voice.state", Json{{"player", index}, {"talking", talking != 0}});
}
