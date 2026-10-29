#include "engine.h"

#include <ncl_perf/profiler.h>

void CL_Stop_f()
{
    NCL_PROFILE_SCOPE();

    eng()->CL_Stop_f.InvokeChained();
}
