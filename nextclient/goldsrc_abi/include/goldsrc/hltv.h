#pragma once

#include <goldsrc/fwd.h>

namespace goldsrc
{
    inline constexpr int DRC_CMD_NONE = 0;
    inline constexpr int DRC_CMD_START = 1;
    inline constexpr int DRC_CMD_EVENT = 2;
    inline constexpr int DRC_CMD_MODE = 3;
    inline constexpr int DRC_CMD_CAMERA = 4;
    inline constexpr int DRC_CMD_TIMESCALE = 5;
    inline constexpr int DRC_CMD_MESSAGE = 6;
    inline constexpr int DRC_CMD_SOUND = 7;
    inline constexpr int DRC_CMD_STATUS = 8;
    inline constexpr int DRC_CMD_BANNER = 9;
    inline constexpr int DRC_CMD_STUFFTEXT = 10;
    inline constexpr int DRC_CMD_CHASE = 11;
    inline constexpr int DRC_CMD_INEYE = 12;
    inline constexpr int DRC_CMD_MAP = 13;
    inline constexpr int DRC_CMD_CAMPATH = 14;
    inline constexpr int DRC_CMD_LAST = 15;
    inline constexpr int DRC_CMD_WAYPOINTS = 15;
    inline constexpr int DRC_FLAG_STARTPATH = 0x00000001;
    inline constexpr int DRC_FLAG_SLOWSTART = 0x00000002;
    inline constexpr int DRC_FLAG_SLOWEND = 0x00000004;
    inline constexpr int DRC_FLAG_PRIO_MASK = 0x0000000F;
    inline constexpr int DRC_FLAG_SIDE = 0x00000010;
    inline constexpr int DRC_FLAG_DRAMATIC = 0x00000020;
    inline constexpr int DRC_FLAG_SLOWMOTION = 0x00000040;
    inline constexpr int DRC_FLAG_FACEPLAYER = 0x00000080;
    inline constexpr int DRC_FLAG_INTRO = 0x00000100;
    inline constexpr int DRC_FLAG_FINAL = 0x00000200;
    inline constexpr int DRC_FLAG_NO_RANDOM = 0x00000400;
    inline constexpr int HLTV_ACTIVE = 0;
    inline constexpr int HLTV_STATUS = 1;
    inline constexpr int HLTV_LISTEN = 2;
    inline constexpr int TYPE_CLIENT = 0;
    inline constexpr int TYPE_PROXY = 1;
    inline constexpr int TYPE_DIRECTOR = 2;
    inline constexpr int TYPE_COMMENTATOR = 3;
    inline constexpr int TYPE_DEMO = 4;
} // namespace goldsrc
