#pragma once
#include <cctype>

namespace nitro_utils
{
    inline int _fast_to_lower(char c)
    {
        int i = (unsigned char)c;
        if (i < 0x80)
        {
            // Brutally fast branchless ASCII tolower():
            i += (((('A' - 1) - i) & (i - ('Z' + 1))) >> 26) & 0x20;
        }
        else
        {
            i += isupper(i) ? 0x20 : 0;
        }
        return i;
    }
} // namespace nitro_utils
