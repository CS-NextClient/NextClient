#include "MasterDetailsByAddress.h"

using namespace service::matchmaking;

namespace
{
    constexpr uint32_t kPortBits = 16;
} // namespace

void MasterDetailsByAddress::Rebuild(const std::vector<MasterServerEntry>& entries)
{
    details_by_address_.clear();

    for (const MasterServerEntry& entry : entries)
    {
        if (entry.details.game_mode.empty() && entry.details.country_code.empty())
        {
            continue;
        }

        uint64_t key = AddressKey(entry.address.GetIPHostByteOrder(), entry.address.GetPortHostByteOrder());
        details_by_address_[key] = entry.details;
    }
}

const MasterDetails* MasterDetailsByAddress::Find(uint32_t ip, uint16_t port) const
{
    auto it = details_by_address_.find(AddressKey(ip, port));

    return it != details_by_address_.end() ? &it->second : nullptr;
}

uint64_t MasterDetailsByAddress::AddressKey(uint32_t ip, uint16_t port)
{
    return (static_cast<uint64_t>(ip) << kPortBits) | port;
}
