#include <utils/platform.h>
#include <filesystem>
#include <format>

#ifndef _WIN32
    #include <cstring>
#endif

std::filesystem::path GetCurrentProcessPath()
{
#ifdef _WIN32
    char module_path[MAX_PATH];
    GetModuleFileNameA(nullptr, module_path, sizeof(module_path));

    auto current_path = std::filesystem::path(module_path);
#else
    auto current_path = std::filesystem::read_symlink("/proc/self/exe");
#endif

    return current_path;
}

std::filesystem::path GetCurrentProcessPathAbsoulute()
{
    return absolute(GetCurrentProcessPath());
}

std::filesystem::path GetCurrentProcessDirectory()
{
    return GetCurrentProcessPath().parent_path();
}

std::filesystem::path GetCurrentProcessDirectoryAbsoulute()
{
    return absolute(GetCurrentProcessDirectory());
}

#ifdef _WIN32

std::string GetWinErrorString(DWORD error)
{
    char buf[256] = { '\0' };

    DWORD len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, error, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                   buf, sizeof(buf), NULL);

    if (len == 0)
        return std::format("code {}, !!! FormatMessageA failed, code {} !!!", error, GetLastError());

    while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n'))
        len--;

    buf[len] = '\0';
    return std::format("code {}, {}", error, buf);
}

#else

std::string GetWinErrorString(int error)
{
    return std::format("code {}, {}", error, strerror(error));
}

#endif
