
#include "nitroapi/utility/ContainerWithHandles.h"

namespace nitroapi
{
    static utility::ContainerWithHandles<int, void*> g_HandlersStorage;

    utility::ContainerWithHandles<int, void*>& HandlersStorage() { return g_HandlersStorage; }
}