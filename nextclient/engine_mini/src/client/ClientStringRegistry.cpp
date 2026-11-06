#include "client/ClientStringRegistry.h"

#include <utility>

namespace
{
    constexpr size_t kMaxCacheSize = 2048;
}

const char* ClientStringRegistry::Find(uint16_t string_id) const
{
    auto found = entries_.find(string_id);
    return found != entries_.end() ? found->second.c_str() : nullptr;
}

ClientStringRegistry::RegisterResult ClientStringRegistry::Register(uint16_t string_id, std::string value)
{
    if (string_id == 0)
    {
        return RegisterResult::Ignored;
    }

    if (entries_.size() >= kMaxCacheSize && !entries_.contains(string_id))
    {
        return RegisterResult::CapacityExceeded;
    }

    entries_[string_id] = std::move(value);
    return RegisterResult::Registered;
}

void ClientStringRegistry::Clear()
{
    entries_.clear();
}
