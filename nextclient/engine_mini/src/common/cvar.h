#pragma once

extern cvar_t** p_cvar_vars;

void Cvar_Set(const char *name, const char *value);
cvar_t* Cvar_FindVar(const char *name);
