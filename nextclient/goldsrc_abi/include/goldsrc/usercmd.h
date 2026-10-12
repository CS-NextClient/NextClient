#pragma once

#include <cstdint>

#include <goldsrc/fwd.h>
#include <goldsrc/types.h>

namespace goldsrc
{
    struct kbutton_t
    {
        int down[2];
        int state;
    };

    struct usercmd_t
    {
        short lerp_msec;
        uint8_t msec;
        vec3_t viewangles;
        float forwardmove;
        float sidemove;
        float upmove;
        uint8_t lightlevel;
        unsigned short buttons;
        uint8_t impulse;
        uint8_t weaponselect;
        int impact_index;
        vec3_t impact_position;
    };
} // namespace goldsrc
