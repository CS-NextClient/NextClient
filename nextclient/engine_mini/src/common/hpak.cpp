#include <goldsrc/engine/resource.h>
#include <goldsrc/types.h>

#include "../engine.h"

bool HPAK_GetDataPointer(const char* filename, goldsrc::resource_t* pResource, uint8_t** buffer, int* bufsize)
{
    return eng()->HPAK_GetDataPointer.InvokeChained(filename, pResource, buffer, bufsize);
}

void HPAK_AddLump(goldsrc::qboolean bUseQueue, const char* pakname, goldsrc::resource_t* pResource, uint8_t* pData, FileHandle_t fpSource)
{
    eng()->HPAK_AddLump.InvokeChained(bUseQueue, pakname, pResource, pData, fpSource);
}

void HPAK_FlushHostQueue()
{
    eng()->HPAK_FlushHostQueue.InvokeChained();
}
