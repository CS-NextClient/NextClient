#include <nitro_utils/platform.h>
#include <cstring>
#include <link.h>
#include <sys/mman.h>
#include <pmparser.h>

namespace nitro_utils
{
    SysModule GetSysModule(const char *name)
    {
        void* handle;

        handle = dlopen(name, RTLD_NOLOAD);

        if (handle == nullptr)
            return nullptr;

        return handle;
    }

    bool GetSysModuleInfo(SysModule module, uint32_t* start_address, uint32_t* size)
    {
        link_map* linkmap = nullptr;
        if (dlinfo((void*) module, RTLD_DI_LINKMAP, &linkmap))
            return false;

        procmaps_iterator* maps = pmparser_parse(-1);
        if (maps == nullptr)
            return false;

        procmaps_struct* maps_tmp = nullptr;
        while ((maps_tmp = pmparser_next(maps)) != nullptr)
        {
            if (std::strcmp(linkmap->l_name, maps_tmp->pathname) == 0)
            {
                *start_address = (uint32_t)maps_tmp->addr_start;
                *size = (uint32_t)maps_tmp->length;

                pmparser_free(maps);

                return true;
            }
        }

        pmparser_free(maps);

        return false;
    }

    SysModule LoadSysModule(const char* name)
    {
        void* module  = nullptr;
        char szAbsoluteModuleName[1024];
        if (name[0] != '/')
        {
            char szCwd[1024];
            getcwd(szCwd, sizeof(szCwd));
            if (szCwd[strlen(szCwd) - 1] == '/')
                szCwd[strlen(szCwd) - 1] = '\0';

            snprintf(szAbsoluteModuleName, sizeof(szAbsoluteModuleName), "%s/%s", szCwd, name);
            module = dlopen(szAbsoluteModuleName, RTLD_NOW);
        }
        else
        {
            snprintf(szAbsoluteModuleName, sizeof(szAbsoluteModuleName), "%s", name);
            module = dlopen(name, RTLD_NOW);
        }

        if (!module)
        {
            char str[512];

            printf("Error: %s\n", dlerror());
            snprintf(str, sizeof(str), "%s.so", szAbsoluteModuleName);
            module = dlopen(str, RTLD_NOW);
        }

        return module;
    }

    void UnloadSysModule(SysModule module)
    {
        if (dlopen(nullptr, RTLD_LAZY) == module)
            return;

        dlclose(module);
    }

    void* GetProcAddress(SysModule module, const char* func_name)
    {
        return dlsym(module, func_name);
    }

    bool SetProtect(const void *addr, size_t size, ProtectMode protectMode)
    {
        int mode;
        switch (protectMode)
        {
            case ProtectMode::PROTECT_RW:
                mode = PROT_READ | PROT_WRITE;
                break;

            case ProtectMode::PROTECT_RWE:
                mode = PROT_READ | PROT_WRITE | PROT_EXEC;
                break;

            case ProtectMode::PROTECT_RE:
                mode = PROT_READ | PROT_EXEC;
                break;

            default:
                return false;
        }

        size_t pageSize = sysconf(_SC_PAGESIZE);
        size_t iaddr = reinterpret_cast<size_t>(addr);
        size_t roundAddr = iaddr & ~(pageSize - static_cast<size_t>(1));

#ifndef NDEBUG
        if (pageSize != 4096) fprintf(stderr, "large page(%zd) is used. not tested enough.\n", pageSize);
#endif

        return mprotect(reinterpret_cast<void*>(roundAddr), size + (iaddr - roundAddr), mode) == 0;
    }
}
