#include <nitro_utils/platform.h>
#include <Windows.h>
#include <psapi.h>

namespace nitro_utils
{
    SysModule GetSysModule(const char *name)
    {
        return GetModuleHandleA(name);
    }

    bool GetSysModuleInfo(SysModule module, uint32_t* start_address, uint32_t* size)
    {
        MODULEINFO module_info;
        if (GetModuleInformation(GetCurrentProcess(), (HMODULE)module, &module_info, sizeof(module_info)))
        {
            *start_address = (uint32_t)module_info.lpBaseOfDll;
            *size = module_info.SizeOfImage;

            return true;
        }

        return false;
    }

    SysModule LoadSysModule(const char* name)
    {
        return LoadLibraryA(name);
    }

    void UnloadSysModule(SysModule module)
    {
        if (GetModuleHandleA(nullptr) == module)
            return;

        FreeLibrary((HMODULE)module);
    }

    void* GetProcAddress(SysModule module, const char* func_name)
    {
        return ::GetProcAddress((HMODULE)module, func_name);
    }

    bool SetProtect(const void *addr, size_t size, ProtectMode protectMode)
    {
        DWORD mode;
        switch (protectMode)
        {
            case ProtectMode::PROTECT_RW:
                mode = PAGE_READWRITE;
                break;

            case ProtectMode::PROTECT_RWE:
                mode = PAGE_EXECUTE_READWRITE;
                break;

            case ProtectMode::PROTECT_RE:
                mode = PAGE_EXECUTE_READ;
                break;

            default:
                return false;
        }

        DWORD oldProtect;
        return VirtualProtect(const_cast<void*>(addr), size, mode, &oldProtect) != 0;
    }
}