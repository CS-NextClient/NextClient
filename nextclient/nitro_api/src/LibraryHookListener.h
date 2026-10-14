#pragma once

#include <nitro_utils/platform.h>

namespace nitroapi
{
    class LibraryHookListener
    {
    public:
        virtual ~LibraryHookListener() = default;

        virtual void OnLibraryLoaded(nitro_utils::SysModule hModule) = 0;
        virtual void OnLibraryUnload() = 0;
    };
}