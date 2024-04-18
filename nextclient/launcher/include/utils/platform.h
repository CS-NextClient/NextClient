#pragma once

#include <filesystem>
#include <Windows.h>

std::filesystem::path GetCurrentProcessPath();
std::filesystem::path GetCurrentProcessPathAbsoulute();
std::filesystem::path GetCurrentProcessDirectory();
std::filesystem::path GetCurrentProcessDirectoryAbsoulute();
std::string GetWinErrorString(DWORD error);
