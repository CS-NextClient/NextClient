#pragma once

#include <goldsrc/fwd.h>
#include <goldsrc/types.h>

namespace goldsrc
{
#if defined(_WIN32)
    struct alignas(8) server_static_t
#elif defined(__linux__)
    struct alignas(4) server_static_t
#endif
    {
        qboolean dll_initialized;
        client_s* clients;
        int maxclients;
#if defined(_WIN32)
        unsigned char unmodelled_[136 - (8 + sizeof(maxclients))];
#elif defined(__linux__)
        unsigned char unmodelled_[132 - (8 + sizeof(maxclients))];
#endif
    };

#if defined(_WIN32)
    struct alignas(8) server_t
#elif defined(__linux__)
    struct alignas(4) server_t
#endif
    {
        qboolean active;
#if defined(_WIN32)
        unsigned char unmodelled_[287768 - (0 + sizeof(active))];
#elif defined(__linux__)
        unsigned char unmodelled_[287756 - (0 + sizeof(active))];
#endif
    };
} // namespace goldsrc
