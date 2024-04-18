#pragma once

#include <string>
#include <next_launcher/IUserStorage.h>
#include "RegistryEx.h"

class RegistryUserStorage : public next_launcher::IUserStorage
{
    const char* kNextRegistryStorageLegacySec2 = "Software\\Valve\\Half-Life\\cstrike16\\sec2";

    CRegistryEx local_registry_;
    CRegistryEx global_registry_;
    CRegistryEx legacy_sec2_registry_;

    std::string GetLocalRegistrySection(std::string context);

public:
    explicit RegistryUserStorage(std::string base_section);
    ~RegistryUserStorage() override;

    int GetLocal(const char* key, int defaultValue) override;
    void GetLocal(const char* key, char* output, int max_len, const char* defaultValue) override;
    void SetLocal(const char* key, int value) override;
    void SetLocal(const char* key, const char* value) override;
    void DeleteLocalKey(const char* key) override;
    void DeleteAllLocalKeys(const char* key) override;

    int GetGlobal(const char* key, int defaultValue) override;
    void GetGlobal(const char* key, char* output, int max_len, const char* defaultValue) override;
    void SetGlobal(const char* key, int value) override;
    void SetGlobal(const char* key, const char* value) override;
    void DeleteGlobalKey(const char* key) override;
    void DeleteAllGlobalKeys(const char* key) override;
};
