#include "sys_dll.h"

double Sys_FloatTime()
{
    return eng()->Sys_FloatTime.InvokeChained();
}

float GetCvarFloat(const char* cvar)
{
    return eng()->cl_enginefunc->pfnGetCvarFloat(cvar);
}

const char* GetCvarString(const char* cvar)
{
    return eng()->cl_enginefunc->pfnGetCvarString(cvar);
}
