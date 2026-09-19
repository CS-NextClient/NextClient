#pragma once

#include <cstring>

#include <goldsrc/client_api.h>
#include <goldsrc/cstrike/hud.h>
#include <goldsrc/fwd.h>
#include <goldsrc/types.h>

// mathlib's Vector, declared with the class-key of its definition: MSVC warns about a
// mismatch (C4099) and encodes class and struct differently in decorated names.
class Vector;

namespace nitroapi
{
    // Pointers into the HUD object of client.dll.
    struct gamehud_t
    {
        bool initialized{};
        goldsrc::cstrike::HUDLIST** m_pHudList{};
        goldsrc::client_sprite_t** m_pSpriteList{};
        int* m_iSpriteCount{};
        int* m_iSpriteCountAllRes{};
        float* m_flMouseSensitivity{};
        float* m_flTime{};
        float* m_fOldTime{};
        double* m_flTimeDelta{};
        Vector* m_vecOrigin{};
        Vector* m_vecAngles{};
        int* m_iKeyBits{};
        int* m_iHideHUDDisplay{};
        int* m_iFOV{};
        int* m_Teamplay{};
        int* m_iRes{};
        int (*m_pPlayerFOV)[64]{};
        int* m_iFontHeight{};
        int* m_iFontEngineHeight{};
        goldsrc::HSPRITE_t** m_rghSprites{};
        goldsrc::wrect_t** m_rgrcRects{};
        char** m_rgszSpriteNames{};
        int* m_iWeaponBits{};
        bool* m_fPlayerDead{};
        int* m_iIntermission{};
        int* m_HUD_number_0{};
        float* m_flCheatCheckTime{};
        float* m_flExpensiveCheckTime{};
        bool* m_bRenderGunSmoke{};
        float* m_fGLTEXSORT{};
        float* m_flGLZMAX{};
        bool* m_bSoftwaremode{};
        bool* m_bShowTimer{};
        bool* m_autoBuyStringSent{};
        bool* m_rebuyStringSent{};
        goldsrc::cstrike::CHudAmmo* m_Ammo{};
        goldsrc::cstrike::CHudHealth* m_Health{};
        goldsrc::cstrike::CHudSpectator* m_Spectator{};
        goldsrc::cstrike::CHudGeiger* m_Geiger{};
        goldsrc::cstrike::CHudBattery* m_Battery{};
        goldsrc::cstrike::CHudTrain* m_Train{};
        goldsrc::cstrike::CHudFlashlight* m_Flash{};
        goldsrc::cstrike::CHudMessage* m_Message{};
        goldsrc::cstrike::CHudStatusBar* m_StatusBar{};
        goldsrc::cstrike::CHudDeathNotice* m_DeathNotice{};
        goldsrc::cstrike::CHudSayText* m_SayText{};
        goldsrc::cstrike::CHudMenu* m_Menu{};
        goldsrc::cstrike::CHudAmmoSecondary* m_AmmoSecondary{};
        goldsrc::cstrike::CHudTextMessage* m_TextMessage{};
        goldsrc::cstrike::CHudStatusIcons* m_StatusIcons{};
        goldsrc::cstrike::CHudAccountBalance* m_accountBalance{};
        goldsrc::cstrike::CHudRoundTimer* m_roundTimer{};
        goldsrc::cstrike::CHudProgressBar* m_progressBar{};
        goldsrc::cstrike::CHudNightVision* m_NightVision{};
        goldsrc::cstrike::CHudCareerTaskTimer* m_careerTaskTimer{};
        goldsrc::cstrike::CHudScenarioStatus* m_scenarioStatus{};
        goldsrc::cstrike::CHudSniperScope* m_sniperScope{};
        goldsrc::cstrike::CHudVGUI2Print* m_VGUI2Print{};
        goldsrc::SCREENINFO* m_scrinfo{};

        goldsrc::HSPRITE_t GetSprite(int index) const
        {
            if (index < 0)
            {
                return 0;
            }

            return (*m_rghSprites)[index];
        }

        goldsrc::wrect_t& GetSpriteRect(int index) const
        {
            return (*m_rgrcRects)[index];
        }

        int GetSpriteWidth(int index) const
        {
            const goldsrc::wrect_t& rect = GetSpriteRect(index);

            return rect.right - rect.left;
        }

        int GetSpriteHeight(int index) const
        {
            const goldsrc::wrect_t& rect = GetSpriteRect(index);

            return rect.bottom - rect.top;
        }

        int GetSpriteIndex(const char* name) const
        {
            for (int index = 0; index < *m_iSpriteCount; ++index)
            {
                const char* sprite_name = *m_rgszSpriteNames + index * goldsrc::cstrike::MAX_SPRITE_NAME_LENGTH;

                if (std::strncmp(name, sprite_name, goldsrc::cstrike::MAX_SPRITE_NAME_LENGTH) == 0)
                {
                    return index;
                }
            }

            return -1;
        }
    };
} // namespace nitroapi
