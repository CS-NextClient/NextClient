#pragma once

#include <goldsrc/fwd.h>
#include <goldsrc/types.h>

namespace goldsrc
{
    inline constexpr int CHAN_AUTO = 0;
    inline constexpr int CHAN_WEAPON = 1;
    inline constexpr int CHAN_VOICE = 2;
    inline constexpr int CHAN_ITEM = 3;
    inline constexpr int CHAN_BODY = 4;
    inline constexpr int CHAN_STREAM = 5;
    inline constexpr int CHAN_STATIC = 6;
    inline constexpr int CHAN_NETWORKVOICE_BASE = 7;
    inline constexpr int CHAN_NETWORKVOICE_END = 500;
    inline constexpr int CHAN_BOT = 501;
    inline constexpr int CVOXFILESENTENCEMAX = 1536;

    struct alignas(4) sfx_t
    {
        char name[64];
        cache_user_t cache;
        unsigned char unmodelled_[72 - (64 + sizeof(cache))];
    };
} // namespace goldsrc
