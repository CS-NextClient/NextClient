#include "../engine.h"
#include <ncl_perf/profiler.h>

void S_ExtraUpdate()
{
    NCL_PROFILE_SCOPE();

    eng()->S_ExtraUpdate.InvokeChained();
}