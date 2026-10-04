#include "DiscordUrlScheme.h"

#include "console/console.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#endif

#include <string>

#ifdef _WIN32

static bool SetRegistryString(const std::wstring& key, const wchar_t* name, const std::wstring& value)
{
    DWORD size = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    return RegSetKeyValueW(HKEY_CURRENT_USER, key.c_str(), name, REG_SZ, value.c_str(), size) == ERROR_SUCCESS;
}

void DiscordUrlScheme_Register(const char* app_id)
{
    wchar_t exe_path[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
        return;

    // app_id is all digits, so widening char by char is enough
    std::wstring key = L"Software\\Classes\\discord-";
    for (const char* c = app_id; *c != '\0'; c++)
        key += static_cast<wchar_t>(*c);

    std::wstring command = L"\"" + std::wstring(exe_path) + L"\"";

    bool ok = SetRegistryString(key, nullptr, L"URL:NextClient")
        && SetRegistryString(key, L"URL Protocol", L"")
        && SetRegistryString(key + L"\\shell\\open\\command", nullptr, command);

    if (!ok)
        Con_Printf("Discord: could not register the join URL scheme\n");
}

#else

void DiscordUrlScheme_Register(const char* app_id)
{
    char exe_path[4096];
    ssize_t length = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    
    if (length <= 0) return;
    exe_path[length] = '\0';

    std::string game_dir(exe_path);
    game_dir.erase(game_dir.rfind('/'));
    
    const passwd* user = getpwuid(getuid());
    if (user == nullptr || user->pw_dir == nullptr)
        return;
    
    std::string home = user->pw_dir;

    std::string data_dir;
    const char* xdg_data_home = getenv("XDG_DATA_HOME");

    data_dir = (xdg_data_home != nullptr && xdg_data_home[0] != '\0')
        ? xdg_data_home
        : home + "/.local/share";

    std::string applications_dir = data_dir + "/applications";
    std::error_code error;
    std::filesystem::create_directories(applications_dir, error);
    if (error)
        return;

    std::string desktop_path = applications_dir + "/discord-" + app_id + ".desktop";
    FILE* file = fopen(desktop_path.c_str(), "w");
    if (!file)
        return;

    fprintf(file, "[Desktop Entry]\n");
    fprintf(file, "Type=Application\n");
    fprintf(file, "Name=NextClient\n");
    fprintf(file, "NoDisplay=true\n");
    fprintf(file, "Exec=\"%s/nextclient.sh\" %%u\n", game_dir.c_str());
    fprintf(file, "MimeType=x-scheme-handler/discord-%s;\n", app_id);
    fclose(file);

    std::string command = "env -u LD_LIBRARY_PATH HOME=" + home + " xdg-mime default discord-" + app_id
        + ".desktop x-scheme-handler/discord-" + app_id;
    if (system(command.c_str()) != 0)
        Con_Printf("Discord: could not register the join URL scheme\n");
}

#endif
