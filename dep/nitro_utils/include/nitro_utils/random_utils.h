#pragma once

#include <vector>
#include <random>

namespace nitro_utils
{
#ifdef _WIN32
    std::vector<uint8_t> GenerateRandomBytesSecure(size_t count);
#endif

    // [from, to]
    template<class T = int>
    T GenerateRandomNum(T from, T to)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<T> dis(from, to);

        return dis(gen);
    }
}