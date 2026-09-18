#pragma once

namespace goldsrc::cstrike
{
    inline constexpr int WEAPON_IS_ONTARGET = 0x00000040;

    enum WeaponIdType
    {
        WEAPON_NONE = 0,
        WEAPON_P228 = 1,
        WEAPON_GLOCK = 2,
        WEAPON_SCOUT = 3,
        WEAPON_HEGRENADE = 4,
        WEAPON_XM1014 = 5,
        WEAPON_C4 = 6,
        WEAPON_MAC10 = 7,
        WEAPON_AUG = 8,
        WEAPON_SMOKEGRENADE = 9,
        WEAPON_ELITE = 10,
        WEAPON_FIVESEVEN = 11,
        WEAPON_UMP45 = 12,
        WEAPON_SG550 = 13,
        WEAPON_GALIL = 14,
        WEAPON_FAMAS = 15,
        WEAPON_USP = 16,
        WEAPON_GLOCK18 = 17,
        WEAPON_AWP = 18,
        WEAPON_MP5N = 19,
        WEAPON_M249 = 20,
        WEAPON_M3 = 21,
        WEAPON_M4A1 = 22,
        WEAPON_TMP = 23,
        WEAPON_G3SG1 = 24,
        WEAPON_FLASHBANG = 25,
        WEAPON_DEAGLE = 26,
        WEAPON_SG552 = 27,
        WEAPON_AK47 = 28,
        WEAPON_KNIFE = 29,
        WEAPON_P90 = 30,
        WEAPON_SHIELDGUN = 99
    };

    enum WeaponState
    {
        WPNSTATE_USP_SILENCED = 1,
        WPNSTATE_GLOCK18_BURST_MODE = 2,
        WPNSTATE_M4A1_SILENCED = 4,
        WPNSTATE_ELITE_LEFT = 8,
        WPNSTATE_FAMAS_BURST_MODE = 16,
        WPNSTATE_SHIELD_DRAWN = 32
    };

    enum elite_e
    {
        ELITE_IDLE = 0,
        ELITE_IDLE_LEFTEMPTY = 1,
        ELITE_SHOOTLEFT1 = 2,
        ELITE_SHOOTLEFT2 = 3,
        ELITE_SHOOTLEFT3 = 4,
        ELITE_SHOOTLEFT4 = 5,
        ELITE_SHOOTLEFT5 = 6,
        ELITE_SHOOTLEFTLAST = 7,
        ELITE_SHOOTRIGHT1 = 8,
        ELITE_SHOOTRIGHT2 = 9,
        ELITE_SHOOTRIGHT3 = 10,
        ELITE_SHOOTRIGHT4 = 11,
        ELITE_SHOOTRIGHT5 = 12,
        ELITE_SHOOTRIGHTLAST = 13,
        ELITE_RELOAD = 14,
        ELITE_DRAW = 15
    };

    enum glock18_e
    {
        GLOCK18_IDLE1 = 0,
        GLOCK18_IDLE2 = 1,
        GLOCK18_IDLE3 = 2,
        GLOCK18_SHOOT = 3,
        GLOCK18_SHOOT2 = 4,
        GLOCK18_SHOOT3 = 5,
        GLOCK18_SHOOT_EMPTY = 6,
        GLOCK18_RELOAD = 7,
        GLOCK18_DRAW = 8,
        GLOCK18_HOLSTER = 9,
        GLOCK18_ADD_SILENCER = 10,
        GLOCK18_DRAW2 = 11,
        GLOCK18_RELOAD2 = 12
    };

    enum m4a1_e
    {
        M4A1_IDLE = 0,
        M4A1_SHOOT1 = 1,
        M4A1_SHOOT2 = 2,
        M4A1_SHOOT3 = 3,
        M4A1_RELOAD = 4,
        M4A1_DRAW = 5,
        M4A1_ATTACH_SILENCER = 6,
        M4A1_UNSIL_IDLE = 7,
        M4A1_UNSIL_SHOOT1 = 8,
        M4A1_UNSIL_SHOOT2 = 9,
        M4A1_UNSIL_SHOOT3 = 10,
        M4A1_UNSIL_RELOAD = 11,
        M4A1_UNSIL_DRAW = 12,
        M4A1_DETACH_SILENCER = 13
    };

    enum usp_e
    {
        USP_IDLE = 0,
        USP_SHOOT1 = 1,
        USP_SHOOT2 = 2,
        USP_SHOOT3 = 3,
        USP_SHOOT_EMPTY = 4,
        USP_RELOAD = 5,
        USP_DRAW = 6,
        USP_ATTACH_SILENCER = 7,
        USP_UNSIL_IDLE = 8,
        USP_UNSIL_SHOOT1 = 9,
        USP_UNSIL_SHOOT2 = 10,
        USP_UNSIL_SHOOT3 = 11,
        USP_UNSIL_SHOOT_EMPTY = 12,
        USP_UNSIL_RELOAD = 13,
        USP_UNSIL_DRAW = 14,
        USP_DETACH_SILENCER = 15
    };
} // namespace goldsrc::cstrike
