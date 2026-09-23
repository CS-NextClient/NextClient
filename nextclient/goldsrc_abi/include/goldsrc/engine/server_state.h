#pragma once

#include <goldsrc/fwd.h>
#include <goldsrc/types.h>

namespace goldsrc
{
    struct alignas(8) server_static_t
    {
        qboolean dll_initialized;
        client_s* clients;
        int maxclients;
        unsigned char unmodelled_[136 - (8 + sizeof(maxclients))];
    };

    struct alignas(8) server_t
    {
        qboolean active;
        unsigned char unmodelled_[287768 - (0 + sizeof(active))];
    };
} // namespace goldsrc
