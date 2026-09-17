#include <goldsrc/engine/resource.h>
#include <goldsrc/types.h>

#include "common.h"
#include "../engine.h"

goldsrc::qboolean COM_CreateCustomization(goldsrc::customization_t* pListHead,
                                          goldsrc::resource_t* pResource,
                                          int playernumber,
                                          int flags,
                                          goldsrc::customization_t** pCustomization,
                                          int* nLumps)
{
    return eng()->COM_CreateCustomization.InvokeChained(pListHead, pResource, playernumber, flags, pCustomization, nLumps);
}
