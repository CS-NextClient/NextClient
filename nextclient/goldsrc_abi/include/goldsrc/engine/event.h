#pragma once

#include <goldsrc/event_api.h>
#include <goldsrc/fwd.h>

namespace goldsrc
{
    struct event_info_s
    {
        unsigned short index;
        short packet_index;
        short entity_index;
        float fire_time;
        event_args_t args;
        int flags;
    };

    struct event_t
    {
        unsigned short index;
        const char* filename;
        int filesize;
        const char* pszScript;
    };

    struct event_state_t
    {
        event_info_s ei[64];
    };
} // namespace goldsrc
