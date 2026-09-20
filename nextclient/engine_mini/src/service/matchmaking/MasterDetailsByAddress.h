#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "service/matchmaking/master/MasterServerEntry.h"

namespace service::matchmaking
{
    // What the last master server list reported, by server address
    class MasterDetailsByAddress
    {
        std::unordered_map<uint64_t, MasterDetails> details_by_address_{};

    public:
        void Rebuild(const std::vector<MasterServerEntry>& entries);
        [[nodiscard]] const MasterDetails* Find(uint32_t ip, uint16_t port) const;

    private:
        static uint64_t AddressKey(uint32_t ip, uint16_t port);
    };
} // namespace service::matchmaking
