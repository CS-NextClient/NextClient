#include "DiscordValidation.h"

#include <cctype>
#include <cstring>

bool IsSafeServerAddress(const char* address)
{
    if (address == nullptr)
        return false;

    size_t length = strlen(address);
    if (length == 0 || length > 63)
        return false;

    for (size_t i = 0; i < length; i++)
    {
        unsigned char c = address[i];

        if (!isalnum(c) && c != '.' && c != ':' && c != '-')
            return false;
    }

    return true;
}
