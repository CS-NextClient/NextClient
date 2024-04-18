#pragma once

#ifdef _DEBUG
#define ASSERT(exp)	if(!( exp )) Sys_Error( "assert failed at %s:%i\n", __FILE__, __LINE__ )
#else
#define ASSERT(exp)	((void)0)
#endif

template<class... TArgs>
void Sys_Error(const char* error, TArgs&&... args) { eng()->Sys_Error.GetFunc()(error, std::forward<TArgs>(args)...); }

template<class... TArgs>
char* Con_Printf(const char* format, TArgs&&... args) { return eng()->Con_Printf.GetFunc()(format, std::forward<TArgs>(args)...); }

template<class... TArgs>
char* Con_DPrintf(const char* format, TArgs&&... args) { return eng()->Con_DPrintf.GetFunc()(format, std::forward<TArgs>(args)...); }
