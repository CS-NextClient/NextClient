#pragma once

#include <cstdint>

#include <goldsrc/fwd.h>
#include <goldsrc/types.h>

namespace goldsrc
{
    struct ref_params_t
    {
        float vieworg[3];
        float viewangles[3];
        float forward[3];
        float right[3];
        float up[3];
        float frametime;
        float time;
        int intermission;
        int paused;
        int spectator;
        int onground;
        int waterlevel;
        float simvel[3];
        float simorg[3];
        float viewheight[3];
        float idealpitch;
        float cl_viewangles[3];
        int health;
        float crosshairangle[3];
        float viewsize;
        float punchangle[3];
        int maxclients;
        int viewentity;
        int playernum;
        int max_entities;
        int demoplayback;
        int hardware;
        int smoothing;
        usercmd_t* cmd;
        movevars_s* movevars;
        int viewport[4];
        int nextView;
        int onlyClientDraw;
    };

    struct refdef_t
    {
        vrect_t vrect;
        vrect_t aliasvrect;
        int vrectright;
        int vrectbottom;
        int aliasvrectright;
        int aliasvrectbottom;
        float vrectrightedge;
        float fvrectx;
        float fvrecty;
        float fvrectx_adj;
        float fvrecty_adj;
        int vrect_x_adj_shift20;
        int vrectright_adj_shift20;
        float fvrectright_adj;
        float fvrectbottom_adj;
        float fvrectright;
        float fvrectbottom;
        float horizontalFieldOfView;
        float xOrigin;
        float yOrigin;
        vec3_t vieworg;
        vec3_t viewangles;
        color24 ambientlight;
        qboolean onlyClientDraws;
    };

    struct screenfade_t
    {
        float fadeSpeed;
        float fadeEnd;
        float fadeTotalEnd;
        float fadeReset;
        uint8_t fader;
        uint8_t fadeg;
        uint8_t fadeb;
        uint8_t fadealpha;
        int fadeFlags;
    };
} // namespace goldsrc
