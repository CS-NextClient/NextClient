#include "DiscordValidation.h"

namespace
{
    // Reads a decimal number at p no bigger than max and moves p past its digits
    bool ReadNumber(const char*& p, unsigned int max)
    {
        if (*p < '0' || *p > '9')
        {
            return false;
        }

        unsigned int value = 0;
        int digits = 0;

        while (*p >= '0' && *p <= '9')
        {
            value = value * 10 + (*p - '0');
            p++;

            if (++digits > 5 || value > max)
            {
                return false;
            }
        }

        return true;
    }
} // namespace

bool Discord_IsSafeJoinAddress(const char* address)
{
    if (address == nullptr)
    {
        return false;
    }

    const char* p = address;

    for (int i = 0; i < 4; i++)
    {
        if (!ReadNumber(p, 255))
        {
            return false;
        }

        char separator = i < 3 ? '.' : ':';
        if (*p != separator)
        {
            return false;
        }
        p++;
    }

    return ReadNumber(p, 65535) && *p == '\0';
}
