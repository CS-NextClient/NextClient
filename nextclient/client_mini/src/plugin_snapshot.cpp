#include "plugin_snapshot.h"

#include <cstring>

#include "main.h"

using json_t = tao::json::value;

namespace
{
    template <size_t N>
    std::string ArrayText(const char (&value)[N])
    {
        return {value, strnlen_s(value, N)};
    }
} // namespace

json_t PluginSnapshot_Vector(const float* values, size_t count)
{
    json_t result = tao::json::empty_array;
    for (size_t i = 0; i < count; ++i)
    {
        result.push_back(values[i]);
    }
    return result;
}
json_t PluginSnapshot_Entity(const entity_state_t& state)
{
    json_t result = tao::json::empty_object;
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
    result["origin"] = PluginSnapshot_Vector(state.origin);
    result["angles"] = PluginSnapshot_Vector(state.angles);
    result["velocity"] = PluginSnapshot_Vector(state.velocity);
    result["mins"] = PluginSnapshot_Vector(state.mins);
    result["maxs"] = PluginSnapshot_Vector(state.maxs);
    result["basevelocity"] = PluginSnapshot_Vector(state.basevelocity);
    result["startpos"] = PluginSnapshot_Vector(state.startpos);
    result["endpos"] = PluginSnapshot_Vector(state.endpos);
    result["vuser1"] = PluginSnapshot_Vector(state.vuser1);
    result["vuser2"] = PluginSnapshot_Vector(state.vuser2);
    result["vuser3"] = PluginSnapshot_Vector(state.vuser3);
    result["vuser4"] = PluginSnapshot_Vector(state.vuser4);

    result["rendercolor"] = json_t::array({state.rendercolor.r, state.rendercolor.g, state.rendercolor.b});
    result["controller"] = json_t::array({state.controller[0], state.controller[1], state.controller[2], state.controller[3]});
    result["blending"] = json_t::array({state.blending[0], state.blending[1], state.blending[2], state.blending[3]});
    return result;
}
json_t PluginSnapshot_Client(const clientdata_t& state)
{
    json_t result = tao::json::empty_object;
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
    result["origin"] = PluginSnapshot_Vector(state.origin);
    result["velocity"] = PluginSnapshot_Vector(state.velocity);
    result["punchangle"] = PluginSnapshot_Vector(state.punchangle);
    result["view_ofs"] = PluginSnapshot_Vector(state.view_ofs);
    result["vuser1"] = PluginSnapshot_Vector(state.vuser1);
    result["vuser2"] = PluginSnapshot_Vector(state.vuser2);
    result["vuser3"] = PluginSnapshot_Vector(state.vuser3);
    result["vuser4"] = PluginSnapshot_Vector(state.vuser4);

    result["physinfo"] = ArrayText(state.physinfo);
    return result;
}
json_t PluginSnapshot_Movement(const playermove_t& movement)
{
    json_t result = tao::json::empty_object;
    result["player_index"] = movement.player_index;
    result["multiplayer"] = movement.multiplayer;
    result["time"] = movement.time;
    result["frametime"] = movement.frametime;
    result["flDuckTime"] = movement.flDuckTime;
    result["bInDuck"] = movement.bInDuck;
    result["flTimeStepSound"] = movement.flTimeStepSound;
    result["iStepLeft"] = movement.iStepLeft;
    result["flFallVelocity"] = movement.flFallVelocity;
    result["flSwimTime"] = movement.flSwimTime;
    result["flNextPrimaryAttack"] = movement.flNextPrimaryAttack;
    result["effects"] = movement.effects;
    result["flags"] = movement.flags;
    result["usehull"] = movement.usehull;
    result["gravity"] = movement.gravity;
    result["friction"] = movement.friction;
    result["oldbuttons"] = movement.oldbuttons;
    result["waterjumptime"] = movement.waterjumptime;
    result["dead"] = movement.dead;
    result["deadflag"] = movement.deadflag;
    result["spectator"] = movement.spectator;
    result["movetype"] = movement.movetype;
    result["onground"] = movement.onground;
    result["waterlevel"] = movement.waterlevel;
    result["watertype"] = movement.watertype;
    result["oldwaterlevel"] = movement.oldwaterlevel;
    result["maxspeed"] = movement.maxspeed;
    result["clientmaxspeed"] = movement.clientmaxspeed;
    result["iuser1"] = movement.iuser1;
    result["iuser2"] = movement.iuser2;
    result["iuser3"] = movement.iuser3;
    result["iuser4"] = movement.iuser4;
    result["fuser1"] = movement.fuser1;
    result["fuser2"] = movement.fuser2;
    result["fuser3"] = movement.fuser3;
    result["fuser4"] = movement.fuser4;
    result["forward"] = PluginSnapshot_Vector(movement.forward);
    result["right"] = PluginSnapshot_Vector(movement.right);
    result["up"] = PluginSnapshot_Vector(movement.up);
    result["origin"] = PluginSnapshot_Vector(movement.origin);
    result["angles"] = PluginSnapshot_Vector(movement.angles);
    result["oldangles"] = PluginSnapshot_Vector(movement.oldangles);
    result["velocity"] = PluginSnapshot_Vector(movement.velocity);
    result["movedir"] = PluginSnapshot_Vector(movement.movedir);
    result["basevelocity"] = PluginSnapshot_Vector(movement.basevelocity);
    result["view_ofs"] = PluginSnapshot_Vector(movement.view_ofs);
    result["punchangle"] = PluginSnapshot_Vector(movement.punchangle);
    result["vuser1"] = PluginSnapshot_Vector(movement.vuser1);
    result["vuser2"] = PluginSnapshot_Vector(movement.vuser2);
    result["vuser3"] = PluginSnapshot_Vector(movement.vuser3);
    result["vuser4"] = PluginSnapshot_Vector(movement.vuser4);
    result["texture"] = ArrayText(movement.sztexturename);
    result["texture_type"] = std::string(1, movement.chtexturetype);
    return result;
}
