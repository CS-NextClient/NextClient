#pragma once
#include <memory>
#include <string>
#include <next_launcher/IUserInfo.h>
#include <next_launcher/IUserStorage.h>

class DefaultUserInfo : public next_launcher::IUserInfo
{
    std::shared_ptr<next_launcher::IUserStorage> user_storage_;
    char uid_[64] = {};

public:
    explicit DefaultUserInfo(std::shared_ptr<next_launcher::IUserStorage> user_storage);

    void IncreaseLaunchGameCountAndSave() override;
    int GetLaunchGameCount() override;

    void GetUpdateBranch(char* branch, int len) override;
    void SetUpdateBranch(const char* branch) override;

    void GetScreenDimension(char *dimension, int dimension_size) override;
    void GetClientUid(char *uid, int uid_size) override;
    void GetOs(char *os, int os_size) override;
    
private:
    void InitId();
};