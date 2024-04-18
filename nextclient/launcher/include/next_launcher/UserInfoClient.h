#pragma once

#include "IUserInfo.h"
#include <string>

namespace next_launcher
{
    class UserInfoClient
    {
        IUserInfo* user_info_interface_;

    public:
        explicit UserInfoClient(IUserInfo* user_info_interface) : user_info_interface_(user_info_interface) { }

        void IncreaseLaunchGameCountAndSave() { user_info_interface_->IncreaseLaunchGameCountAndSave(); }
        [[nodiscard]] int GetLaunchGameCount() { return user_info_interface_->GetLaunchGameCount(); }
        [[nodiscard]] std::string GetScreenDimension() { return GetStringVal(&IUserInfo::GetScreenDimension); }
        [[nodiscard]] std::string GetClientUid() { return GetStringVal(&IUserInfo::GetClientUid); }
        [[nodiscard]] std::string GetOs() { return GetStringVal(&IUserInfo::GetOs); }

        [[nodiscard]] std::string GetUpdateBranch() { return GetStringVal(&IUserInfo::GetUpdateBranch); }
        void SetUpdateBranch(std::string branch) { user_info_interface_->SetUpdateBranch(branch.c_str()); }

        [[nodiscard]] IUserInfo* GetInterface() { return user_info_interface_; }

    private:
        template<class TMethod>
        std::string GetStringVal(TMethod method)
        {
            char buffer[1024];
            (user_info_interface_->*method)(buffer, sizeof(buffer));

            return buffer;
        }
    };
}