#pragma once

#include <cstdlib>
#include <cstdint>

namespace nitro_utils
{
    using SysModule = void*;

    enum class ProtectMode {
        PROTECT_RW = 0, // read/write
        PROTECT_RWE = 1, // read/write/exec
        PROTECT_RE = 2 // read/exec
    };

    SysModule GetSysModule(const char* name);
    bool GetSysModuleInfo(SysModule module, uint32_t* start_address, uint32_t* size);
    SysModule LoadSysModule(const char* name);
    void UnloadSysModule(SysModule module);
    void* GetProcAddress(SysModule module, const char* func_name);

    bool SetProtect(const void *addr, size_t size, ProtectMode protectMode);
}

#ifdef _WIN32
#include "platform/platform_win32.h"
#else
#include "platform/platform_linux.h"
#endif
