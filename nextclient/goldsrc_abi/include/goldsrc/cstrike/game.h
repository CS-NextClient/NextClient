#pragma once

#include <goldsrc/fwd.h>

namespace goldsrc::cstrike
{
    inline constexpr int DMG_HALLUC = static_cast<int>(0x80000000);
    inline constexpr int DMG_GENERIC = 0x00000000;
    inline constexpr int DMG_CRUSH = 0x00000001;
    inline constexpr int DMG_BULLET = 0x00000002;
    inline constexpr int DMG_NAIL = 0x00000004;
    inline constexpr int DMG_SLASH = 0x00000004;
    inline constexpr int DMG_BURN = 0x00000008;
    inline constexpr int DMG_FREEZE = 0x00000010;
    inline constexpr int DMG_NOT_SELF = 0x00000010;
    inline constexpr int DMG_FALL = 0x00000020;
    inline constexpr int DMG_BLAST = 0x00000040;
    inline constexpr int DMG_CLUB = 0x00000080;
    inline constexpr int DMG_SHOCK = 0x00000100;
    inline constexpr int DMG_CONCUSS = 0x00000200;
    inline constexpr int DMG_SONIC = 0x00000200;
    inline constexpr int DMG_GIB_CORPSE = 0x000002E1;
    inline constexpr int DMG_ENERGYBEAM = 0x00000400;
    inline constexpr int DMG_NEVERGIB = 0x00001000;
    inline constexpr int DMG_ALWAYSGIB = 0x00002000;
    inline constexpr int DMG_DROWN = 0x00004000;
    inline constexpr int DMG_FIRSTTIMEBASED = 0x00004000;
    inline constexpr int DMG_PARALYZE = 0x00008000;
    inline constexpr int DMG_NERVEGAS = 0x00010000;
    inline constexpr int DMG_POISON = 0x00020000;
    inline constexpr int DMG_RADIATION = 0x00040000;
    inline constexpr int DMG_DROWNRECOVER = 0x00080000;
    inline constexpr int DMG_ACID = 0x00100000;
    inline constexpr int DMG_SLOWBURN = 0x00200000;
    inline constexpr int DMG_SLOWFREEZE = 0x00400000;
    inline constexpr int DMG_SHOWNHUD = 0x00774118;
    inline constexpr int DMG_MORTAR = 0x00800000;
    inline constexpr int DMG_TRANQ = 0x00800000;
    inline constexpr unsigned int DMG_TIMEBASED = 0x00FFC000u;
    inline constexpr int DMG_EXPLOSION = 0x01000000;
    inline constexpr int DMG_IGNITE = 0x01000000;
    inline constexpr int DMG_IGNORE_MAXHEALTH = 0x01000000;
    inline constexpr int DMG_RADIUS_MAX = 0x02000000;
    inline constexpr int DMG_RADIUS_QUAKE = 0x04000000;
    inline constexpr int DMG_IGNOREARMOR = 0x08000000;
    inline constexpr int DMG_AIMED = 0x10000000;
    inline constexpr int DMG_WALLPIERCING = 0x20000000;
    inline constexpr int DMG_CALTROP = 0x40000000;
    inline constexpr int HIDEHUD_WEAPONS = 0x00000001;
    inline constexpr int HIDEHUD_FLASHLIGHT = 0x00000002;
    inline constexpr int HIDEHUD_ALL = 0x00000004;
    inline constexpr int HIDEHUD_HEALTH = 0x00000008;
    inline constexpr int HIDEHUD_TIMER = 0x00000010;
    inline constexpr int HIDEHUD_MONEY = 0x00000020;
    inline constexpr int HIDEHUD_CROSSHAIR = 0x00000040;
    inline constexpr int HIDEHUD_OBSERVER_CROSSHAIR = 0x00000080;
    inline constexpr int TEAM_UNASSIGNED = 0;
    inline constexpr int TEAM_TERRORIST = 1;
    inline constexpr int TEAM_CT = 2;
    inline constexpr int TEAM_SPECTATOR = 3;
    inline constexpr int WEAPON_SUIT = 31;
} // namespace goldsrc::cstrike
