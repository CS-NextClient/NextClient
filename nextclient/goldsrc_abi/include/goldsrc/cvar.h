#pragma once

#include <goldsrc/fwd.h>

namespace goldsrc
{
    inline constexpr int FCVAR_ARCHIVE = 0x00000001;
    inline constexpr int FCVAR_USERINFO = 0x00000002;
    inline constexpr int FCVAR_SERVER = 0x00000004;
    inline constexpr int FCVAR_EXTDLL = 0x00000008;
    inline constexpr int FCVAR_CLIENTDLL = 0x00000010;
    inline constexpr int FCVAR_PROTECTED = 0x00000020;
    inline constexpr int FCVAR_SPONLY = 0x00000040;
    inline constexpr int FCVAR_PRINTABLEONLY = 0x00000080;
    inline constexpr int FCVAR_UNLOGGED = 0x00000100;
    inline constexpr int FCVAR_NOEXTRAWHITEPACE = 0x00000200;
    inline constexpr int FCVAR_PRIVILEGED = 0x00000400;
    inline constexpr int FCVAR_FILTERABLE = 0x00000800;
    inline constexpr int FCVAR_ISEXECUTED = 0x00001000;
    inline constexpr int FCVAR_ISPATH = 0x00002000;
    inline constexpr int FCVAR_CHEAT = 0x00100000;

    struct cvar_t
    {
        const char* name;
        char* string;
        int flags;
        float value;
        cvar_t* next;
    };
} // namespace goldsrc
