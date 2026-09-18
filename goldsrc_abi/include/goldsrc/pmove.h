#pragma once

#include <cstdint>

#include <goldsrc/fwd.h>
#include <goldsrc/types.h>
#include <goldsrc/usercmd.h>

namespace goldsrc
{
    inline constexpr int PM_NORMAL = 0x00000000;
    inline constexpr int PM_TRACELINE_PHYSENTSONLY = 0x00000000;
    inline constexpr int PM_STUDIO_IGNORE = 0x00000001;
    inline constexpr int PM_TRACELINE_ANYVISIBLE = 0x00000001;
    inline constexpr int PM_STUDIO_BOX = 0x00000002;
    inline constexpr int PM_GLASS_IGNORE = 0x00000004;
    inline constexpr int PM_WORLD_ONLY = 0x00000008;
    inline constexpr double PM_CHECKSTUCK_MINTIME = 0.05;
    inline constexpr int PM_DEAD_VIEWHEIGHT = -8;
    inline constexpr int PM_PLAYER_FALL_PUNCH_THRESHHOLD = 250;
    inline constexpr int PM_PLAYER_MIN_BOUNCE_SPEED = 350;
    inline constexpr int PM_PLAYER_MAX_SAFE_FALL_SPEED = 580;
    inline constexpr int PM_VEC_HULL_MIN = -36;
    inline constexpr int PM_VEC_DUCK_HULL_MIN = -18;
    inline constexpr int PM_VEC_DUCK_VIEW = 12;
    inline constexpr int PM_VEC_VIEW = 17;

    struct movevars_s
    {
        float gravity;
        float stopspeed;
        float maxspeed;
        float spectatormaxspeed;
        float accelerate;
        float airaccelerate;
        float wateraccelerate;
        float friction;
        float edgefriction;
        float waterfriction;
        float entgravity;
        float bounce;
        float stepsize;
        float maxvelocity;
        float zmax;
        float waveHeight;
        qboolean footsteps;
        char skyName[32];
        float rollangle;
        float rollspeed;
        float skycolor_r;
        float skycolor_g;
        float skycolor_b;
        float skyvec_x;
        float skyvec_y;
        float skyvec_z;
    };

    struct physent_t
    {
        char name[32];
        int player;
        vec3_t origin;
        model_t* model;
        model_t* studiomodel;
        vec3_t mins;
        vec3_t maxs;
        int info;
        vec3_t angles;
        int solid;
        int skin;
        int rendermode;
        float frame;
        int sequence;
        uint8_t controller[4];
        uint8_t blending[2];
        int movetype;
        int takedamage;
        int blooddecal;
        int team;
        int classnumber;
        int iuser1;
        int iuser2;
        int iuser3;
        int iuser4;
        float fuser1;
        float fuser2;
        float fuser3;
        float fuser4;
        vec3_t vuser1;
        vec3_t vuser2;
        vec3_t vuser3;
        vec3_t vuser4;
    };

    struct pmplane_t
    {
        vec3_t normal;
        float dist;
    };

    struct pmtrace_t
    {
        qboolean allsolid;
        qboolean startsolid;
        qboolean inopen;
        qboolean inwater;
        float fraction;
        vec3_t endpos;
        pmplane_t plane;
        int ent;
        vec3_t deltavelocity;
        int hitgroup;
    };

    struct playermove_t
    {
        int player_index;
        qboolean server;
        qboolean multiplayer;
        float time;
        float frametime;
        vec3_t forward;
        vec3_t right;
        vec3_t up;
        vec3_t origin;
        vec3_t angles;
        vec3_t oldangles;
        vec3_t velocity;
        vec3_t movedir;
        vec3_t basevelocity;
        vec3_t view_ofs;
        float flDuckTime;
        qboolean bInDuck;
        int flTimeStepSound;
        int iStepLeft;
        float flFallVelocity;
        vec3_t punchangle;
        float flSwimTime;
        float flNextPrimaryAttack;
        int effects;
        int flags;
        int usehull;
        float gravity;
        float friction;
        int oldbuttons;
        float waterjumptime;
        qboolean dead;
        int deadflag;
        int spectator;
        int movetype;
        int onground;
        int waterlevel;
        int watertype;
        int oldwaterlevel;
        char sztexturename[256];
        char chtexturetype;
        float maxspeed;
        float clientmaxspeed;
        int iuser1;
        int iuser2;
        int iuser3;
        int iuser4;
        float fuser1;
        float fuser2;
        float fuser3;
        float fuser4;
        vec3_t vuser1;
        vec3_t vuser2;
        vec3_t vuser3;
        vec3_t vuser4;
        int numphysent;
        physent_t physents[600];
        int nummoveent;
        physent_t moveents[64];
        int numvisent;
        physent_t visents[600];
        usercmd_t cmd;
        int numtouch;
        pmtrace_t touchindex[600];
        char physinfo[256];
        movevars_s* movevars;
        vec_t _player_mins[4][3];
        vec_t _player_maxs[4][3];
        const char* (*PM_Info_ValueForKey)(const char*, const char*);
        void (*PM_Particle)(float*, int, float, int, int);
        int (*PM_TestPlayerPosition)(float*, pmtrace_t*);
        void (*Con_NPrintf)(int, char*, ...);
        void (*Con_DPrintf)(char*, ...);
        void (*Con_Printf)(char*, ...);
        double (*Sys_FloatTime)();
        void (*PM_StuckTouch)(int, pmtrace_t*);
        int (*PM_PointContents)(float*, int*);
        int (*PM_TruePointContents)(float*);
        int (*PM_HullPointContents)(hull_t*, int, float*);
        pmtrace_t (*PM_PlayerTrace)(float*, float*, int, int);
        pmtrace_t* (*PM_TraceLine)(float*, float*, int, int, int);
        int32_t (*RandomLong)(int32_t, int32_t);
        float (*RandomFloat)(float, float);
        int (*PM_GetModelType)(model_t*);
        void (*PM_GetModelBounds)(model_t*, float*, float*);
        void* (*PM_HullForBsp)(physent_t*, float*);
        float (*PM_TraceModel)(physent_t*, float*, float*, trace_t*);
        int (*COM_FileSize)(char*);
        uint8_t* (*COM_LoadFile)(char*, int, int*);
        void (*COM_FreeFile)(void*);
        char* (*memfgets)(uint8_t*, int, int*, char*, int);
        qboolean runfuncs;
        void (*PM_PlaySound)(int, const char*, float, float, int, int);
        const char* (*PM_TraceTexture)(int, float*, float*);
        void (*PM_PlaybackEventFull)(int, int, unsigned short, float, float*, float*, float, float, int, int, int, int);
        pmtrace_t (*PM_PlayerTraceEx)(float*, float*, int, int (*)(physent_t*));
        int (*PM_TestPlayerPositionEx)(float*, pmtrace_t*, int (*)(physent_t*));
        pmtrace_t* (*PM_TraceLineEx)(float*, float*, int, int, int (*)(physent_t*));
    };
} // namespace goldsrc
