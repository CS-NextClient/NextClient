#pragma once

#ifdef _WIN32
#include <minwindef.h>

namespace nitroapi
{
    LONG WINAPI GlobalExceptionHandlerWin32(EXCEPTION_POINTERS* exception_pointers);
}

#define SEH_TRY __try
#define SEH_CATCH_GLOBAL __except(GlobalExceptionHandlerWin32(GetExceptionInformation()))

#else

#define SEH_TRY if(true)
#define SEH_CATCH_GLOBAL else

#endif