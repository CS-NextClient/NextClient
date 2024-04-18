#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <crc.h>

enum class PrivateResListState
{
    NotActive,
    PrepareToDownloadResList,
    DownloadingResList,
    RerunBatchResources,
    Active
};

struct PrivateResInternal
{
    bool only_client;
    std::string download_file_path;
    CRC32_t server_crc32;
    int size;
};

struct client_stateex_t
{
    // key - resource file path
    std::unordered_map<std::string, PrivateResInternal> privateResources;
    // key - resource download file path, value - set of keys from privateResources
    std::unordered_map<std::string, std::unordered_set<std::string>> privateResourcesReverseCache;
    std::unordered_map<std::string, resource_t> resourcesNeeded;
    std::string privateResListDownloadPath;
    PrivateResListState privateResListState;
};
