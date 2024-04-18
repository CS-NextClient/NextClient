#pragma once

#include "../engine.h"

#ifdef _DEBUG
#define ASSERT(exp)	if(!( exp )) Sys_Error( "assert failed at %s:%i\n", __FILE__, __LINE__ )
#else
#define ASSERT(exp)	((void)0)
#endif

template<class... TArgs>
void Sys_Error(const char* error, TArgs&&... args) { eng()->Sys_Error.GetFunc()(error, std::forward<TArgs>(args)...); }

template<class... TArgs>
void Sys_Printf(const char* format, TArgs&&... args) { eng()->Sys_Printf.GetFunc()(format, std::forward<TArgs>(args)...); }

double Sys_FloatTime();
float GetCvarFloat(const char* cvar);
const char* GetCvarString(const char* cvar);
