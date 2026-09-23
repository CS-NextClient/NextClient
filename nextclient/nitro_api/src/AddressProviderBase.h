#pragma once

#include <functional>
#include <string>
#include <nitroapi/hook/StaticHookId.h>
#include "nitro_utils/MemoryTools.h"
#include "nitro_utils/platform.h"
#include "SearchConfig.h"

namespace nitroapi
{
    class AddressProviderBase
    {
        MemScanner memory_scanner_;
        MemoryModule memory_module_;

    public:
        virtual ~AddressProviderBase() = default;

        uint32_t FindAddress(nitro_utils::SysModule hModule, StaticHookId key);
        uint32_t FindAddress(nitro_utils::SysModule hModule, const std::string &key);

    protected:
        virtual const std::unordered_map<StaticHookId, SearchConfig>& get_addresses_id() = 0;
        virtual const std::unordered_map<std::string, SearchConfig>& get_addresses_str() = 0;

    private:
        void InitScanner(nitro_utils::SysModule hModule);
    };
}