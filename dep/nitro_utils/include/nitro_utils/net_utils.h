#pragma once

#include <string>

namespace nitro_utils
{
    bool ParseAddress(const std::string &ip, uint32_t &nIP, uint16_t &nConnPort, bool bDnsResolve = false);
}