#include "common.h"
#include "../engine.h"

qboolean COM_CreateCustomization(customization_t* pListHead, resource_t* pResource, int playernumber, int flags, customization_t** pCustomization, int* nLumps)
{
    return eng()->COM_CreateCustomization.InvokeChained(pListHead, pResource, playernumber, flags, pCustomization, nLumps);
}
