#pragma once

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

    struct sfx_t
    {
        sfx_t() = delete;
        sfx_t(const sfx_t&) = delete;
        sfx_t& operator=(const sfx_t&) = delete;

        char name[64];
        cache_user_t cache;
    };
} // namespace goldsrc
