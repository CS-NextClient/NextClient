#pragma once
#include <netadr.h>

template <>
struct std::hash<netadr_t>
{
    size_t operator()(const netadr_t& key) const noexcept
    {
        return key.GetIPNetworkByteOrder() ^ (key.GetPortNetworkByteOrder() << 16);
    }
};

template <>
struct std::equal_to<netadr_t>
{
    bool operator()(const netadr_t& a, const netadr_t& b) const
    {
        return a.CompareAdr(b);
    }
};
