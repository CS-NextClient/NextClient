#pragma once

#include <goldsrc/client_api.h>
#include <goldsrc/fwd.h>
#include <goldsrc/types.h>

namespace goldsrc::cstrike
{
    inline constexpr int DHN_DRAWZERO = 0x00000001;
    inline constexpr int DHN_2DIGITS = 0x00000002;
    inline constexpr int DHN_3DIGITS = 0x00000004;
    inline constexpr int DHN_PADZERO = 0x00000008;
    inline constexpr int DHN_4DIGITS = 0x00000010;
    inline constexpr int DHN_5DIGITS = 0x00000020;
    inline constexpr int DMG_IMAGE_POISON = 0;
    inline constexpr int DMG_IMAGE_ACID = 1;
    inline constexpr int DMG_IMAGE_COLD = 2;
    inline constexpr int DMG_IMAGE_LIFE = 2;
    inline constexpr int DMG_IMAGE_DROWN = 3;
    inline constexpr int DMG_IMAGE_BURN = 4;
    inline constexpr int DMG_IMAGE_NERVE = 5;
    inline constexpr int DMG_IMAGE_RAD = 6;
    inline constexpr int DMG_IMAGE_SHOCK = 7;
    inline constexpr int DMG_IMAGE_CALTROP = 8;
    inline constexpr int DMG_IMAGE_TRANQ = 9;
    inline constexpr int DMG_IMAGE_CONCUSS = 10;
    inline constexpr int DMG_IMAGE_HALLUC = 11;
    inline constexpr float FADE_TIME = 100.0f;
    inline constexpr int MIN_ALPHA = 100;
    inline constexpr int NUM_DMG_TYPES = 12;
    inline constexpr int RGB_GREENISH = 40960;
    inline constexpr int RGB_REDISH = 16715792;
    inline constexpr int RGB_YELLOWISH = 16752640;

    enum
    {
        MAX_PLAYERS = 64,
        MAX_TEAMS = 64,
        MAX_TEAM_NAME = 16,
        MAX_LOCATION_NAME = 32,
        MAX_HOSTAGES = 24
    };

#pragma pack(push, 8)
    class CHud
    {
    public:
        CHud() = delete;
        CHud(const CHud&) = delete;
        CHud& operator=(const CHud&) = delete;

        HUDLIST* m_pHudList;
        HSPRITE_t m_hsprLogo;
        int m_iLogo;
        client_sprite_t* m_pSpriteList;
        int m_iSpriteCount;
        int m_iSpriteCountAllRes;
        float m_flMouseSensitivity;
        int m_iConcussionEffect;
        HSPRITE_t m_hsprCursor;
        float m_flTime;
        float m_fOldTime;
        double m_flTimeDelta;
        vec3_t m_vecOrigin;
        vec3_t m_vecAngles;
        int m_iKeyBits;
        int m_iHideHUDDisplay;
        int m_iFOV;
        int m_Teamplay;
        int m_iRes;
        cvar_t* m_pCvarStealMouse;
        cvar_t* m_pCvarDraw;
        int m_PlayerFOV[64];
        int m_iFontHeight;
        int m_iFontEngineHeight;
        cvar_t* default_fov;
        HSPRITE_t* m_rghSprites;
        wrect_t* m_rgrcRects;
    };
#pragma pack(pop)

    class CHudBase
    {
    public:
        struct POSITION
        {
            int x;
            int y;
        };

        virtual int Init();
        virtual int VidInit();
        virtual int Draw(float);
        virtual void Think();
        virtual void Reset();
        virtual void InitHUDData();

        CHudBase::POSITION m_pos;
        int m_type;
        int m_iFlags;
    };

    struct extra_player_info_t
    {
        extra_player_info_t() = delete;
        extra_player_info_t(const extra_player_info_t&) = delete;
        extra_player_info_t& operator=(const extra_player_info_t&) = delete;

        short frags;
        short deaths;
        short team_id;
        qboolean has_c4;
        qboolean vip;
        vec3_t origin;
        float radarflash;
        int radarflashon;
        int radarflashes;
        short playerclass;
        short teamnumber;
        char teamname[16];
        bool dead;
        float showhealth;
        int health;
        char location[32];
        int sb_health;
    };
} // namespace goldsrc::cstrike
