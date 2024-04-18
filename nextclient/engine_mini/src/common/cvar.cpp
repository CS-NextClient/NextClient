#include "../engine.h"

cvar_t** p_cvar_vars;

void Cvar_Set(const char* name, const char* value)
{
    eng()->Cvar_Set.InvokeChained(name, value);
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
