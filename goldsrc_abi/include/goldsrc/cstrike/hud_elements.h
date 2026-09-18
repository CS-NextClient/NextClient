#pragma once

#include <goldsrc/client_api.h>
#include <goldsrc/cstrike/hud.h>
#include <goldsrc/fwd.h>
#include <goldsrc/types.h>

namespace goldsrc::cstrike
{
    class CHudAmmo : public CHudBase
    {
    public:
        CHudAmmo() = delete;
        CHudAmmo(const CHudAmmo&) = delete;
        CHudAmmo& operator=(const CHudAmmo&) = delete;

        struct RGBA
        {
            unsigned char r;
            unsigned char g;
            unsigned char b;
            unsigned char a;
        };
    };

    class CHudAmmoSecondary : public CHudBase
    {
    public:
        CHudAmmoSecondary() = delete;
        CHudAmmoSecondary(const CHudAmmoSecondary&) = delete;
        CHudAmmoSecondary& operator=(const CHudAmmoSecondary&) = delete;

        enum
        {
            MAX_SEC_AMMO_VALUES = 4
        };
    };

    class CHudMessage : public CHudBase
    {
    public:
        CHudMessage() = delete;
        CHudMessage(const CHudMessage&) = delete;
        CHudMessage& operator=(const CHudMessage&) = delete;

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
    };

    class CHudNightVision : public CHudBase
    {
    public:
        CHudNightVision() = delete;
        CHudNightVision(const CHudNightVision&) = delete;
        CHudNightVision& operator=(const CHudNightVision&) = delete;

        int m_fOn;
    };

    class CHudRoundTimer : public CHudBase
    {
    public:
        CHudRoundTimer() = delete;
        CHudRoundTimer(const CHudRoundTimer&) = delete;
        CHudRoundTimer& operator=(const CHudRoundTimer&) = delete;

        float m_flTimeEnd;
        int m_HUD_stopwatch;
        float m_fFade;
        float m_flNewPeriod;
        float m_flNextToggle;
        int m_bPanicColor;
        int m_closestRight;
    };

    class CHudStatusBar : public CHudBase
    {
    public:
        CHudStatusBar() = delete;
        CHudStatusBar(const CHudStatusBar&) = delete;
        CHudStatusBar& operator=(const CHudStatusBar&) = delete;

        enum
        {
            MAX_STATUSTEXT_LENGTH = 128,
            MAX_STATUSBAR_VALUES = 8,
            MAX_STATUSBAR_LINES = 1
        };
    };

    class CHudStatusIcons : public CHudBase
    {
    public:
        CHudStatusIcons() = delete;
        CHudStatusIcons(const CHudStatusIcons&) = delete;
        CHudStatusIcons& operator=(const CHudStatusIcons&) = delete;

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
        CHudHealth() = delete;
        CHudHealth(const CHudHealth&) = delete;
        CHudHealth& operator=(const CHudHealth&) = delete;

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
