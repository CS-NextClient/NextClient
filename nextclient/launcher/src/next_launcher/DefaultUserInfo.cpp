#include "DefaultUserInfo.h"

#include <sstream>

#include <Windows.h>
#include <Objbase.h>
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
    std::stringstream result;
    result << GetSystemMetrics(SM_CXSCREEN) << "x" << GetSystemMetrics(SM_CYSCREEN);

    V_strncpy(dimension, result.str().c_str(), dimension_size);
}

void DefaultUserInfo::GetClientUid(char *uid, int uid_size)
{
    V_strncpy(uid, uid_, uid_size);
}

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

void DefaultUserInfo::InitId()
{
    user_storage_->GetGlobal("uid", uid_, sizeof(uid_), "");
    if (uid_[0] == '\0')
    {
        GUID guid;
        CoCreateGuid(&guid);

        sprintf_s(uid_,
            "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

        user_storage_->SetGlobal("uid", uid_);
    }
}
