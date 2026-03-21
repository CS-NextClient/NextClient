#include "engine.h"

void W_CleanupName(const char* in, char* out)
{
    int i = 0;
    for (; i < 16 && in[i]; i++)
    {
        int c = in[i];

        out[i] = (c >= 'A' && c <= 'Z') 
            ? c + ('a' - 'A')
            : c;
    }

    for (; i < 16; i++)
    {
        out[i] = '\0';
    }
}

void* W_GetLumpName(int wad, const char* name)
{
    return eng()->W_GetLumpName.InvokeChained(wad, name);
}
