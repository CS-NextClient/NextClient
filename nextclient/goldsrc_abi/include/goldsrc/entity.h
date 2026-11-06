#pragma once

#include <cstdint>

#include <goldsrc/fwd.h>
#include <goldsrc/types.h>

namespace goldsrc
{
    inline constexpr int ET_NORMAL = 0;
    inline constexpr int EF_BRIGHTFIELD = 0x00000001;
    inline constexpr int EF_MUZZLEFLASH = 0x00000002;
    inline constexpr int EF_BRIGHTLIGHT = 0x00000004;
    inline constexpr int EF_DIMLIGHT = 0x00000008;
    inline constexpr int EF_INVLIGHT = 0x00000010;
    inline constexpr int EF_NOINTERP = 0x00000020;
    inline constexpr int EF_LIGHT = 0x00000040;
    inline constexpr int EF_NODRAW = 0x00000080;
    inline constexpr int EF_NIGHTVISION = 0x00000100;
    inline constexpr int EF_SNIPERLASER = 0x00000200;
    inline constexpr int EF_FIBERCAMERA = 0x00000400;
    inline constexpr int FL_DORMANT = static_cast<int>(0x80000000);
    inline constexpr int FL_FLY = 0x00000001;
    inline constexpr int FL_SWIM = 0x00000002;
    inline constexpr int FL_CONVEYOR = 0x00000004;
    inline constexpr int FL_CLIENT = 0x00000008;
    inline constexpr int FL_INWATER = 0x00000010;
    inline constexpr int FL_MONSTER = 0x00000020;
    inline constexpr int FL_GODMODE = 0x00000040;
    inline constexpr int FL_NOTARGET = 0x00000080;
    inline constexpr int FL_SKIPLOCALHOST = 0x00000100;
    inline constexpr int FL_ONGROUND = 0x00000200;
    inline constexpr int FL_PARTIALGROUND = 0x00000400;
    inline constexpr int FL_WATERJUMP = 0x00000800;
    inline constexpr int FL_FROZEN = 0x00001000;
    inline constexpr int FL_FAKECLIENT = 0x00002000;
    inline constexpr int FL_DUCKING = 0x00004000;
    inline constexpr int FL_FLOAT = 0x00008000;
    inline constexpr int FL_GRAPHED = 0x00010000;
    inline constexpr int FL_IMMUNE_WATER = 0x00020000;
    inline constexpr int FL_IMMUNE_SLIME = 0x00040000;
    inline constexpr int FL_IMMUNE_LAVA = 0x00080000;
    inline constexpr int FL_PROXY = 0x00100000;
    inline constexpr int FL_ALWAYSTHINK = 0x00200000;
    inline constexpr int FL_BASEVELOCITY = 0x00400000;
    inline constexpr int FL_MONSTERCLIP = 0x00800000;
    inline constexpr int FL_ONTRAIN = 0x01000000;
    inline constexpr int FL_WORLDBRUSH = 0x02000000;
    inline constexpr int FL_SPECTATOR = 0x04000000;
    inline constexpr int FL_CUSTOMENTITY = 0x20000000;
    inline constexpr int FL_KILLME = 0x40000000;
    inline constexpr int HISTORY_MASK = 63;
    inline constexpr int HISTORY_MAX = 64;
    inline constexpr int MOVETYPE_NONE = 0;
    inline constexpr int MOVETYPE_WALK = 3;
    inline constexpr int MOVETYPE_STEP = 4;
    inline constexpr int MOVETYPE_FLY = 5;
    inline constexpr int MOVETYPE_TOSS = 6;
    inline constexpr int MOVETYPE_PUSH = 7;
    inline constexpr int MOVETYPE_NOCLIP = 8;
    inline constexpr int MOVETYPE_FLYMISSILE = 9;
    inline constexpr int MOVETYPE_BOUNCE = 10;
    inline constexpr int MOVETYPE_BOUNCEMISSILE = 11;
    inline constexpr int MOVETYPE_FOLLOW = 12;
    inline constexpr int MOVETYPE_PUSHSTEP = 13;
    inline constexpr int SOLID_NOT = 0;
    inline constexpr int SOLID_TRIGGER = 1;
    inline constexpr int SOLID_BBOX = 2;
    inline constexpr int SOLID_SLIDEBOX = 3;
    inline constexpr int SOLID_BSP = 4;

    enum
    {
        kRenderNormal = 0,
        kRenderTransColor = 1,
        kRenderTransTexture = 2,
        kRenderGlow = 3,
        kRenderTransAlpha = 4,
        kRenderTransAdd = 5
    };

    enum
    {
        kRenderFxNone = 0,
        kRenderFxPulseSlow = 1,
        kRenderFxPulseFast = 2,
        kRenderFxPulseSlowWide = 3,
        kRenderFxPulseFastWide = 4,
        kRenderFxFadeSlow = 5,
        kRenderFxFadeFast = 6,
        kRenderFxSolidSlow = 7,
        kRenderFxSolidFast = 8,
        kRenderFxStrobeSlow = 9,
        kRenderFxStrobeFast = 10,
        kRenderFxStrobeFaster = 11,
        kRenderFxFlickerSlow = 12,
        kRenderFxFlickerFast = 13,
        kRenderFxNoDissipation = 14,
        kRenderFxDistort = 15,
        kRenderFxHologram = 16,
        kRenderFxDeadPlayer = 17,
        kRenderFxExplode = 18,
        kRenderFxGlowShell = 19,
        kRenderFxClampMinScale = 20,
        kRenderFxLightMultiplier = 21
    };

    struct clientdata_t
    {
        vec3_t origin;
        vec3_t velocity;
        int viewmodel;
        vec3_t punchangle;
        int flags;
        int waterlevel;
        int watertype;
        vec3_t view_ofs;
        float health;
        int bInDuck;
        int weapons;
        int flTimeStepSound;
        int flDuckTime;
        int flSwimTime;
        int waterjumptime;
        float maxspeed;
        float fov;
        int weaponanim;
        int m_iId;
        int ammo_shells;
        int ammo_nails;
        int ammo_cells;
        int ammo_rockets;
        float m_flNextAttack;
        int tfstate;
        int pushmsec;
        int deadflag;
        char physinfo[256];
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

    struct entity_state_t
    {
        int entityType;
        int number;
        float msg_time;
        int messagenum;
        vec3_t origin;
        vec3_t angles;
        int modelindex;
        int sequence;
        float frame;
        int colormap;
        short skin;
        short solid;
        int effects;
        float scale;
        uint8_t eflags;
        int rendermode;
        int renderamt;
        color24 rendercolor;
        int renderfx;
        int movetype;
        float animtime;
        float framerate;
        int body;
        uint8_t controller[4];
        uint8_t blending[4];
        vec3_t velocity;
        vec3_t mins;
        vec3_t maxs;
        int aiment;
        int owner;
        float friction;
        float gravity;
        int team;
        int playerclass;
        int health;
        qboolean spectator;
        int weaponmodel;
        int gaitsequence;
        vec3_t basevelocity;
        int usehull;
        int oldbuttons;
        int onground;
        int iStepLeft;
        float flFallVelocity;
        float fov;
        int weaponanim;
        vec3_t startpos;
        vec3_t endpos;
        float impacttime;
        float starttime;
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

    struct latchedvars_t
    {
        float prevanimtime;
        float sequencetime;
        uint8_t prevseqblending[2];
        vec3_t prevorigin;
        vec3_t prevangles;
        int prevsequence;
        float prevframe;
        uint8_t prevcontroller[4];
        uint8_t prevblending[2];
    };

    struct mouth_t
    {
        uint8_t mouthopen;
        uint8_t sndcount;
        int sndavg;
    };

    struct position_history_t
    {
        float animtime;
        vec3_t origin;
        vec3_t angles;
    };

    struct weapon_data_t
    {
        int m_iId;
        int m_iClip;
        float m_flNextPrimaryAttack;
        float m_flNextSecondaryAttack;
        float m_flTimeWeaponIdle;
        int m_fInReload;
        int m_fInSpecialReload;
        float m_flNextReload;
        float m_flPumpTime;
        float m_fReloadTime;
        float m_fAimedDamage;
        float m_fNextAimBonus;
        int m_fInZoom;
        int m_iWeaponState;
        int iuser1;
        int iuser2;
        int iuser3;
        int iuser4;
        float fuser1;
        float fuser2;
        float fuser3;
        float fuser4;
    };

    struct cl_entity_t
    {
        int index;
        qboolean player;
        entity_state_t baseline;
        entity_state_t prevstate;
        entity_state_t curstate;
        int current_position;
        position_history_t ph[64];
        mouth_t mouth;
        latchedvars_t latched;
        float lastmove;
        vec3_t origin;
        vec3_t angles;
        vec3_t attachment[4];
        int trivial_accept;
        model_t* model;
        efrag_t* efrag;
        mnode_t* topnode;
        float syncbase;
        int visframe;
        colorVec cvFloorColor;
    };

    struct local_state_t
    {
        entity_state_t playerstate;
        clientdata_t client;
        weapon_data_t weapondata[64];
    };
} // namespace goldsrc
