#include "engine.h"

#include <optick.h>

void CL_Stop_f()
{
    OPTICK_EVENT();

    eng()->CL_Stop_f.InvokeChained();
}
