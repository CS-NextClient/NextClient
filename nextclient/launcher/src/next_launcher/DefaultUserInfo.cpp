#include "DefaultUserInfo.h"

#include <sstream>

#ifdef _WIN32
    #include <Windows.h>
    #include <Objbase.h>
#else
    #include <cstdio>
    #include <fstream>
    #include <nitro_utils/random_utils.h>
#endif
#include <strtools.h>

DefaultUserInfo::DefaultUserInfo(std::shared_ptr<next_launcher::IUserStorage> user_storage) :
    user_storage_(user_storage)
{
    InitId();
}

void DefaultUserInfo::IncreaseLaunchGameCountAndSave()
{
    int launch_game_count = user_storage_->GetGlobal("LaunchGameCount", 0);
    user_storage_->SetGlobal("LaunchGameCount", launch_game_count + 1);
}

int DefaultUserInfo::GetLaunchGameCount()
{
    return user_storage_->GetGlobal("LaunchGameCount", 0);
}

void DefaultUserInfo::GetUpdateBranch(char* branch, int len) {
    user_storage_->GetLocal("branch", branch, len, "main");
}

void DefaultUserInfo::SetUpdateBranch(const char* branch) {
    user_storage_->SetLocal("branch", branch);
}

void DefaultUserInfo::GetScreenDimension(char *dimension, int dimension_size)
{
#ifdef _WIN32
    std::stringstream result;
    result << GetSystemMetrics(SM_CXSCREEN) << "x" << GetSystemMetrics(SM_CYSCREEN);

    V_strncpy(dimension, result.str().c_str(), dimension_size);
#else
    // Not currently called from anywhere and querying the display without pulling
    // in SDL2/X11 here isn't worth it for an unused analytics field.
    V_strncpy(dimension, "unknown", dimension_size);
#endif
}

void DefaultUserInfo::GetClientUid(char *uid, int uid_size)
{
    V_strncpy(uid, uid_, uid_size);
}

#ifdef _WIN32

void DefaultUserInfo::GetOs(char *os, int os_size)
{
    DWORD data_size = os_size;

    RegGetValueA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        "ProductName",
        RRF_RT_REG_SZ,
        nullptr,
        os,
        &data_size);
}

#else

// No registry to read a product name from - /etc/os-release's PRETTY_NAME is the
// standard cross-distro equivalent.
void DefaultUserInfo::GetOs(char *os, int os_size)
{
    std::ifstream file("/etc/os-release");
    std::string line;

    while (std::getline(file, line))
    {
        if (!line.starts_with("PRETTY_NAME="))
            continue;

        std::string value = line.substr(strlen("PRETTY_NAME="));
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            value = value.substr(1, value.size() - 2);

        V_strncpy(os, value.c_str(), os_size);
        return;
    }

    V_strncpy(os, "Linux", os_size);
}

#endif

void DefaultUserInfo::InitId()
{
    user_storage_->GetGlobal("uid", uid_, sizeof(uid_), "");
    if (uid_[0] == '\0')
    {
#ifdef _WIN32
        GUID guid;
        CoCreateGuid(&guid);

        sprintf_s(uid_,
            "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
#else
        // No CoCreateGuid outside Windows - just fill in the same GUID-shaped string
        // with our own random bytes, nothing reads this as a real Microsoft GUID.
        uint8_t bytes[16];
        for (uint8_t& byte : bytes)
            byte = nitro_utils::GenerateRandomNum<int>(0, 255);

        snprintf(uid_, sizeof(uid_),
            "%02hhX%02hhX%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
            bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
            bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
#endif

        user_storage_->SetGlobal("uid", uid_);
    }
}
