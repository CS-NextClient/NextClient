#pragma once

#include "../../chain/HandlersChainInterface.h"
#include "../../NitroFunction.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace nitroapi
{
    struct WindowsData
    {
#ifdef _WIN32
        NitroFunction<HMODULE, LPCSTR>  LoadLibraryAFunc;
        NitroFunction<HMODULE, LPCWSTR> LoadLibraryWFunc;
        NitroFunction<BOOL, HMODULE>    FreeLibraryFunc;
        NitroFunction<HCURSOR, HCURSOR> SetCursorFunc;
#endif
    };
}
