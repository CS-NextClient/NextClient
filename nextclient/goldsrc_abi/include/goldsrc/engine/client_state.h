#pragma once

#include <cstdint>

#include <goldsrc/engine/event.h>
#include <goldsrc/engine/net.h>
#include <goldsrc/engine/resource.h>
#include <goldsrc/entity.h>
#include <goldsrc/fwd.h>
#include <goldsrc/types.h>
#include <goldsrc/usercmd.h>
#include <goldsrc/view.h>

namespace goldsrc
{
    enum CareerStateType
    {
        CAREER_NONE = 0,
        CAREER_LOADING = 1,
        CAREER_PLAYING = 2
    };

    enum cactive_t
    {
        ca_dedicated = 0,
        ca_disconnected = 1,
        ca_connecting = 2,
        ca_connected = 3,
        ca_uninitialized = 4,
        ca_active = 5
    };

    struct cmd_t
    {
        usercmd_t cmd;
        float senttime;
        float receivedtime;
        float frame_lerp;
        qboolean processedfuncs;
        qboolean heldback;
        int sendsize;
    };

    struct consistency_t
    {
        char* filename;
        int issound;
        int orig_index;
        int value;
        int check_type;
        float mins[3];
        float maxs[3];
    };

    struct packet_entities_t
    {
        int num_entities;
        unsigned char flags[32];
        entity_state_t* entities;
    };

    struct player_info_t
    {
        int userid;
        char userinfo[256];
        char name[32];
        int spectator;
        int ping;
        int packet_loss;
        char model[64];
        int topcolor;
        int bottomcolor;
        int renderframe;
        int gaitsequence;
        float gaitframe;
        float gaityaw;
        vec3_t prevgaitorigin;
        customization_t customdata;
        char hashedcdkey[16];
        uint64_t m_nSteamID;
    };

    struct soundfade_t
    {
        int nStartPercent;
        int nClientSoundFadePercent;
        double soundFadeStartTime;
        int soundFadeOutTime;
        int soundFadeHoldTime;
        int soundFadeInTime;
    };

    struct client_static_t
    {
        cactive_t state;
        netchan_t netchan;
        sizebuf_t datagram;
        uint8_t datagram_buf[4000];
        double connect_time;
        int connect_retry;
        int challenge;
        uint8_t authprotocol;
        int userid;
        char trueaddress[32];
        float slist_time;
        int signon;
        char servername[260];
        char mapstring[64];
        char spawnparms[2048];
        char userinfo[256];
        float nextcmdtime;
        int lastoutgoingcommand;
        int demonum;
        char demos[32][16];
        qboolean demorecording;
        qboolean demoplayback;
        qboolean timedemo;
        float demostarttime;
        int demostartframe;
        int forcetrack;
        void* demofile;
        void* demoheader;
        qboolean demowaiting;
        qboolean demoappending;
        char demofilename[260];
        int demoframecount;
        int td_lastframe;
        int td_startframe;
        float td_starttime;
        incomingtransfer_t dl;
        float packet_loss;
        double packet_loss_recalc_time;
        int playerbits;
        soundfade_t soundfade;
        char physinfo[256];
        unsigned char md5_clientdll[16];
        netadr_t game_stream;
        netadr_t connect_stream;
        qboolean passive;
        qboolean spectator;
        qboolean director;
        qboolean fSecureClient;
        qboolean isVAC2Secure;
        uint64_t GameServerSteamID;
        int build_num;
    };

    struct frame_t
    {
        double receivedtime;
        double latency;
        qboolean invalid;
        qboolean choked;
        entity_state_t playerstate[32];
        double time;
        clientdata_t clientdata;
        weapon_data_t weapondata[64];
        packet_entities_t packet_entities;
        uint16_t clientbytes;
        uint16_t playerinfobytes;
        uint16_t packetentitybytes;
        uint16_t tentitybytes;
        uint16_t soundbytes;
        uint16_t eventbytes;
        uint16_t usrbytes;
        uint16_t voicebytes;
        uint16_t msgbytes;
    };

    struct client_state_t
    {
        int max_edicts;
        resource_t resourcesonhand;
        resource_t resourcesneeded;
        resource_t resourcelist[1280];
        int num_resources;
        qboolean need_force_consistency_response;
        char serverinfo[512];
        int servercount;
        int validsequence;
        int parsecount;
        int parsecountmod;
        int stats[32];
        int weapons;
        usercmd_t cmd;
        vec3_t viewangles;
        vec3_t punchangle;
        vec3_t crosshairangle;
        vec3_t simorg;
        vec3_t simvel;
        vec3_t simangles;
        vec3_t predicted_origins[64];
        vec3_t prediction_error;
        float idealpitch;
        vec3_t viewheight;
        screenfade_t sf;
        qboolean paused;
        int onground;
        int moving;
        int waterlevel;
        int usehull;
        float maxspeed;
        int pushmsec;
        int light_level;
        int intermission;
        double mtime[2];
        double time;
        double oldtime;
        frame_t frames[64];
        cmd_t commands[64];
        local_state_t predicted_frames[64];
        int delta_sequence;
        int playernum;
        event_t event_precache[256];
        model_t* model_precache[512];
        int model_precache_count;
        sfx_t* sound_precache[512];
        consistency_t consistency_list[512];
        int num_consistency;
        int highentity;
        char levelname[40];
        int maxclients;
        int gametype;
        int viewentity;
        model_t* worldmodel;
        efrag_t* free_efrags;
        int num_entities;
        int num_statics;
        cl_entity_t viewent;
        int cdtrack;
        int looptrack;
        CRC32_t serverCRC;
        unsigned char clientdllmd5[16];
        float weaponstarttime;
        int weaponsequence;
        int fPrecaching;
        dlight_t* pLight;
        player_info_t players[32];
        entity_state_t instanced_baseline[64];
        int instanced_baseline_number;
        CRC32_t mapCRC;
        event_state_t events;
        char downloadUrl[128];
    };
} // namespace goldsrc
