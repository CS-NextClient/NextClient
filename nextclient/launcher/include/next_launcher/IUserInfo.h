#pragma once

namespace next_launcher
{
    class IUserInfo
    {
    public:
        virtual ~IUserInfo() = default;

        virtual void IncreaseLaunchGameCountAndSave() = 0;
        virtual int GetLaunchGameCount() = 0;

        virtual void GetUpdateBranch(char* branch, int len) = 0;
        virtual void SetUpdateBranch(const char* branch) = 0;

        virtual void GetScreenDimension(char* dimension, int dimension_size) = 0;
        virtual void GetClientUid(char* uid, int uid_size) = 0;
        virtual void GetOs(char* os, int os_size) = 0;
    };
}