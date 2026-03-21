#pragma once
#include "engine.h"

extern cvar_t** p_cvar_vars;

typedef void (*pfnCvar_HookVariable_t)(cvar_t*);

typedef struct cvarhook_s
{
    pfnCvar_HookVariable_t hook;
    cvar_t* cvar;
    cvarhook_s* next;
} cvarhook_t;

qboolean Cvar_HookVariable(const char* var_name, cvarhook_t* hook);
void Cvar_RegisterVariable(cvar_t* variable);
void Cvar_Set(const char* name, const char* value);
void Cvar_DirectSet(cvar_t* cvar, const char* value);
cvar_t* Cvar_FindVar(const char* name);
