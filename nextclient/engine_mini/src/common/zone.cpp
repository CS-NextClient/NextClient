#include "../engine.h"
#include <quakedef.h>

void* Mem_Malloc(size_t size)
{
    return eng()->Mem_Malloc(size);
}

void* Mem_ZeroMalloc(size_t size)
{
    return eng()->Mem_ZeroMalloc(size);
}

void Mem_Free(void *p)
{
    eng()->Mem_Free(p);
}

char* Mem_Strdup(const char* strSource)
{
    return eng()->Mem_Strdup(strSource);
}

void* Z_Malloc(int size)
{
    return eng()->Z_Malloc(size);
}

void* Cache_Alloc(cache_user_t* c, int size, char* name)
{
    return eng()->Cache_Alloc(c, size, name);
}

void* Cache_Check(cache_user_t* c)
{
    return eng()->Cache_Check(c);
}

void Cache_Free(cache_user_t* c)
{
    eng()->Cache_Free(c);
}

void* Hunk_AllocName(int size, const char* name)
{
    return eng()->Hunk_AllocName(size, name);
}

void* Hunk_Alloc(int size)
{
    return Hunk_AllocName(size, "unknown");
}

void* Hunk_TempAlloc(int size)
{
    return eng()->Hunk_TempAlloc(size);
}

void Hunk_Check()
{
    eng()->Hunk_Check();
}
