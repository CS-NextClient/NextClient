#include "RegistryUserStorage.h"
#include <filesystem>
#include <strtools.h>
#include <data_encoding/md5.h>
#include <utils/platform.h>

RegistryUserStorage::RegistryUserStorage(std::string base_section) :
    legacy_sec2_registry_(kNextRegistryStorageLegacySec2),
    global_registry_(base_section + "\\global"),
    local_registry_(GetLocalRegistrySection(base_section))
{
    legacy_sec2_registry_.Init();
    global_registry_.Init();
    local_registry_.Init();

    local_registry_.Write("InstallDir", GetCurrentProcessDirectory().string());
}

std::string RegistryUserStorage::GetLocalRegistrySection(std::string context) {
    return context + "\\\\" + md5(GetCurrentProcessDirectory().string()).substr(0, 8);
}

RegistryUserStorage::~RegistryUserStorage() {
    legacy_sec2_registry_.Shutdown();
    global_registry_.Shutdown();
    local_registry_.Shutdown();
}

int RegistryUserStorage::GetLocal(const char* key, int defaultValue) {
    int value = defaultValue;
    if(!local_registry_.Read(key, value) && legacy_sec2_registry_.Read(key, value))
        local_registry_.Write(key, value);

    return value;
}

void RegistryUserStorage::GetLocal(const char* key, char* output, int max_len, const char* defaultValue) {
    std::string value = defaultValue;
    if(!local_registry_.Read(key, value) && legacy_sec2_registry_.Read(key, value))
        local_registry_.Write(key, value);

    V_strncpy(output, value.c_str(), max_len);
}

void RegistryUserStorage::SetLocal(const char* key, int value) {
    local_registry_.Write(key, value);
}

void RegistryUserStorage::SetLocal(const char* key, const char* value) {
    local_registry_.Write(key, value);
}

void RegistryUserStorage::DeleteLocalKey(const char* key) {
    local_registry_.DeleteKey(key);
}

void RegistryUserStorage::DeleteAllLocalKeys(const char* key) {
    // not implemented
}

int RegistryUserStorage::GetGlobal(const char* key, int defaultValue) {
    int value = defaultValue;
    if(!global_registry_.Read(key, value) && legacy_sec2_registry_.Read(key, value))
        global_registry_.Write(key, value);

    return value;
}

void RegistryUserStorage::GetGlobal(const char* key, char* output, int max_len, const char* defaultValue) {
    std::string value = defaultValue;
    if(!global_registry_.Read(key, value) && legacy_sec2_registry_.Read(key, value))
        global_registry_.Write(key, value);

    V_strncpy(output, value.c_str(), max_len);
}

void RegistryUserStorage::SetGlobal(const char* key, int value) {
    global_registry_.Write(key, value);
}

void RegistryUserStorage::SetGlobal(const char* key, const char* value) {
    global_registry_.Write(key, value);
}

void RegistryUserStorage::DeleteGlobalKey(const char* key) {
    global_registry_.DeleteKey(key);
}

void RegistryUserStorage::DeleteAllGlobalKeys(const char* key) {
    // not implemented
}
