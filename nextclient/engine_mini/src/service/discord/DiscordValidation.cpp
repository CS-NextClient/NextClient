#include "DiscordValidation.h"

#include <cctype>
#include <cstring>

#include <common/utf8.h>

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

bool IsValidUtf8(const std::string& text)
{
    if (text.size() == 0)
        return false;

    size_t i = 0;

    while (i < text.size())
    {
        unsigned char c = text[i];
        int length;

        if ((c & 0x80) == 0x00) length = 1;
        else if ((c & 0xE0) == 0xC0) length = 2;
        else if ((c & 0xF0) == 0xE0) length = 3;
        else if ((c & 0xF8) == 0xF0) length = 4;
        else return false;

        if (i + length > text.size())
            return false;

        for (int k = 1; k < length; k++)
        {
            unsigned char next = text[i + k];

            if ((next & kUtf8ContinuationMask) != kUtf8ContinuationBits)
                return false;
        }

        i += length;
    }

    return true;
}
