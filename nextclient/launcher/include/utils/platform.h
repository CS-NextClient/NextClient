#pragma once

#include <filesystem>

#ifdef _WIN32
    #include <Windows.h>
#endif

std::filesystem::path GetCurrentProcessPath();
std::filesystem::path GetCurrentProcessPathAbsoulute();
std::filesystem::path GetCurrentProcessDirectory();
std::filesystem::path GetCurrentProcessDirectoryAbsoulute();

#ifdef _WIN32
    std::string GetWinErrorString(DWORD error);
#else
    std::string GetWinErrorString(int error);
#endif
