#pragma once

#include <goldsrc/client_api.h>
#include <goldsrc/cstrike/hud.h>
#include <goldsrc/fwd.h>
#include <goldsrc/types.h>

namespace goldsrc::cstrike
{
    class alignas(4) CHudAmmo : public CHudBase
    {
    public:
        struct RGBA
        {
            unsigned char r;
            unsigned char g;
            unsigned char b;
            unsigned char a;
        };

        unsigned char unmodelled_[112 - 20];
    };

    class alignas(4) CHudAmmoSecondary : public CHudBase
    {
    public:
        enum
        {
            MAX_SEC_AMMO_VALUES = 4
        };

        unsigned char unmodelled_[44 - 20];
    };

    class alignas(4) CHudMessage : public CHudBase
    {
    public:
        struct client_message_t
        {
            client_textmessage_t* pMessage;
            unsigned long font;
            wchar_t args[64][4];
            int numArgs;
        };

        enum
        {
            MESSAGE_ARG_LEN = 64,
            MAX_MESSAGE_ARGS = 4
        };

#if defined(_WIN32)
        unsigned char unmodelled_[8552 - 20];
#elif defined(__linux__)
        unsigned char unmodelled_[16744 - 20];
#endif
    };

    class alignas(4) CHudNightVision : public CHudBase
    {
    public:
        int m_fOn;
        unsigned char unmodelled_[32 - (20 + sizeof(m_fOn))];
    };

    class CHudRoundTimer : public CHudBase
    {
    public:
        float m_flTimeEnd;
        int m_HUD_stopwatch;
        float m_fFade;
        float m_flNewPeriod;
        float m_flNextToggle;
        int m_bPanicColor;
        int m_closestRight;
    };

    class alignas(4) CHudStatusBar : public CHudBase
    {
    public:
        enum
        {
            MAX_STATUSTEXT_LENGTH = 128,
            MAX_STATUSBAR_VALUES = 8,
            MAX_STATUSBAR_LINES = 1
        };

        unsigned char unmodelled_[316 - 20];
    };

    class alignas(4) CHudStatusIcons : public CHudBase
    {
    public:
        struct icon_sprite_t
        {
            char szSpriteName[24];
            HSPRITE_t spr;
            wrect_t rc;
            unsigned char r;
            unsigned char g;
            unsigned char b;
            qboolean bFlash;
        };

        enum
        {
            MAX_ICONSPRITENAME_LENGTH = 24,
            MAX_ICONSPRITES = 4
        };

        unsigned char unmodelled_[240 - 20];
    };

    struct DAMAGE_IMAGE
    {
        float fExpire;
        float fBaseline;
        int x;
        int y;
    };

    class CHudHealth : public CHudBase
    {
    public:
        int m_iHealth;
        int m_HUD_dmg_bio;
        int m_HUD_cross;
        float m_fFade;
        float m_fAttackFront;
        float m_fAttackRear;
        float m_fAttackLeft;
        float m_fAttackRight;
        bool m_bTrackArray[65];
        int m_iPlayerLastPointedAt;
        int m_iPlayerNum;
        bool m_bDrawRadar;
        HSPRITE_t m_hSprite;
        HSPRITE_t m_hDamage;
        wrect_t* m_hrad;
        wrect_t* m_hradopaque;
        HSPRITE_t m_hRadar;
        HSPRITE_t m_hRadaropaque;
        DAMAGE_IMAGE m_dmg[12];
        int m_bitsDamage;
    };
} // namespace goldsrc::cstrike
