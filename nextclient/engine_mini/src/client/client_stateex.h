#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <ResourceDescriptor.h>

enum class PrivateResListState
{
    NotActive,
    PrepareToDownloadResList,
    DownloadingResList,
    RerunBatchResources,
    Active
};

struct client_stateex_t
{
    // key - ResourceDescriptor::filename_
    std::unordered_map<std::string, ResourceDescriptor> privateResources;
    // key - ResourceDescriptor::download_path_, value - set of keys from client_stateex_t::privateResources
    std::unordered_map<std::string, std::unordered_set<std::string>> privateResourcesReverseCache;
    std::unordered_map<std::string, resource_t> resourcesNeeded;
    std::string privateResListDownloadPath;
    PrivateResListState privateResListState;
};
