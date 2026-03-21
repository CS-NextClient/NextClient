#include "engine.h"
#include "cvar.h"

cvar_t** p_cvar_vars;

qboolean Cvar_HookVariable(const char* var_name, cvarhook_t* hook)
{
    return eng()->Cvar_HookVariable.InvokeChained(var_name, hook);
}

void Cvar_RegisterVariable(cvar_t* variable)
{
    g_engfuncs.pfnCvar_RegisterVariable(variable);
}

void Cvar_Set(const char* name, const char* value)
{
    eng()->Cvar_Set.InvokeChained(name, value);
}

void Cvar_DirectSet(cvar_t* cvar, const char* value)
{
    eng()->Cvar_DirectSet.InvokeChained(cvar, value);
}

cvar_t* Cvar_FindVar(const char* name)
{
    p_g_engdstAddrs->pfnGetCvarPointer(&name);

    cvar_t* cvar;
    for (cvar = *p_cvar_vars; cvar != nullptr; cvar = cvar->next)
    {
        if (!V_stricmp(name, cvar->name))
            break;
    }

    return cvar;
}
