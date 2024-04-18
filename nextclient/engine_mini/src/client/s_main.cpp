#include "../engine.h"
#include <optick.h>

void S_ExtraUpdate()
{
    OPTICK_EVENT();

    eng()->S_ExtraUpdate.InvokeChained();
}