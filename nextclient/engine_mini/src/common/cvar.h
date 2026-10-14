#pragma once
#include <goldsrc/cvar.h>
#include <goldsrc/types.h>

#include "engine.h"

extern goldsrc::cvar_t** p_cvar_vars;

typedef void (*pfnCvar_HookVariable_t)(goldsrc::cvar_t*);

typedef struct cvarhook_s
{
    pfnCvar_HookVariable_t hook;
    goldsrc::cvar_t* cvar;
    cvarhook_s* next;
} cvarhook_t;

goldsrc::qboolean Cvar_HookVariable(const char* var_name, cvarhook_t* hook);
void Cvar_RegisterVariable(goldsrc::cvar_t* variable);
void Cvar_Set(const char* name, const char* value);
void Cvar_DirectSet(goldsrc::cvar_t* cvar, const char* value);
goldsrc::cvar_t* Cvar_FindVar(const char* name);
