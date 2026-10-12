#pragma once

#include "../../AddressProviderBase.h"
#include "../../SearchConfig.h"

namespace nitroapi
{
    class SDL2AddressProvider : public AddressProviderBase
    {
        static const std::unordered_map<StaticHookId, SearchConfig> addresses_hook_id_;
        static const std::unordered_map<std::string, SearchConfig> addresses_str_;

    protected:
        const std::unordered_map<StaticHookId, SearchConfig>& get_addresses_id() override { return addresses_hook_id_; };
        const std::unordered_map<std::string, SearchConfig>& get_addresses_str() override { return addresses_str_; };
    };
}