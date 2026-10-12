#pragma once

#include <vector>
#include <map>
#include <string>
#include "../../dep/nmd/nmd_assembly.h"
#include "platform.h"

struct MemoryModule
{
private:
    uint32_t m_dwStart = 0, m_dwEnd = 0, m_dwSize = 0;
    nitro_utils::SysModule m_hModule = nullptr;
public:
    explicit MemoryModule() { }

    explicit MemoryModule(const char* pszModuleName)
    {
        nitro_utils::SysModule hModule = nitro_utils::GetSysModule(pszModuleName);
        if (hModule)
            Init(hModule);
    };

    explicit MemoryModule(nitro_utils::SysModule hModule)
    {
        Init(hModule);
    };

    void Init(nitro_utils::SysModule hModule)
    {
        if (nitro_utils::GetSysModuleInfo(hModule, &m_dwStart, &m_dwSize))
        {
            m_hModule = hModule;
            m_dwEnd = m_dwStart + m_dwSize;
        }
        else
        {
            m_hModule = nullptr;
        }
    }

    uint32_t Start() const
    {
        return m_dwStart;
    }

    uint32_t End() const
    {
        return m_dwEnd;
    }

    uint32_t Size() const
    {
        return m_dwSize;
    }

    nitro_utils::SysModule Module() const
    {
        return m_hModule;
    }
};

struct memorypattern_t
{
    std::vector<std::uint8_t> m_vecPattern;
    std::vector<std::uint8_t> m_vecMask;
    int m_iDiff = 0;
};

namespace utils
{
    static uint32_t RelativeToAbsolute(uint32_t dwAddress);
    static uint32_t AbsoluteToRelative(uint32_t dwAddress, uint32_t dwAbsRef);
};

class MemScanner
{
    size_t AOBToMaskedPattern(const char* pszPattern, memorypattern_t* pPattern);
    size_t StringToMaskedPattern(const char* pszString, memorypattern_t* pPattern, bool bEOS = true);
    uint32_t m_dwFindStart = 0, m_dwFindEnd = 0;
    nitro_utils::SysModule m_hModule = nullptr;

public:
    MemScanner();
    MemScanner(const MemoryModule &memModule);

    bool SetContext(const MemoryModule &memModule);
    bool AddrInRange(uint32_t dwAddress, uint32_t dwLB = 0, uint32_t dwHB = 0) const;
    uint32_t ModuleStart() const;
    uint32_t ModuleEnd() const;
    nitro_utils::SysModule Module() const { return m_hModule; };

    uint32_t FindPattern(const char* pszPattern, uint32_t dwStart = 0, int32_t iRange = 0);
    uint32_t FindString(const char* pszString, bool bEOS = true, uint32_t dwStart = 0, int32_t iRange = 0);
    uint32_t FindPattern(memorypattern_t* pPattern, uint32_t dwStart = 0, int32_t iRange = 0) const;
    uint32_t FindPattern2(std::string sPattern, uint32_t dwStart = 0) const;
};