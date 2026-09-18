#pragma once

#include <goldsrc/fwd.h>
#include <goldsrc/types.h>

namespace goldsrc
{
    struct server_static_t
    {
        server_static_t() = delete;
        server_static_t(const server_static_t&) = delete;
        server_static_t& operator=(const server_static_t&) = delete;

        qboolean dll_initialized;
        client_s* clients;
        int maxclients;
    };

    struct server_t
    {
        server_t() = delete;
        server_t(const server_t&) = delete;
        server_t& operator=(const server_t&) = delete;

        qboolean active;
    };
} // namespace goldsrc
