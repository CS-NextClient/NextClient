#pragma once

#include <string>
#include <unordered_map>
#include <crc.h>
#include "hlsdk.h"

struct resource_descriptor_t
{
    std::string filename;
    std::string save_path;
    std::string download_path;
    CRC32_t server_crc32 = 0;
    int download_size = 0;
    bool private_resource = false;
    bool only_client = false;
};

resource_descriptor_t ResDesc_Make(const std::string& file_path);
resource_descriptor_t ResDesc_Make(resource_t* resource);
std::vector<resource_descriptor_t> ResDesc_MakeByDownloadPath(const std::string& download_file_path);

// Saves data to the path specified in save_path
bool ResDesc_SaveToFile(const resource_descriptor_t& descriptor, const char* data, int length);
//uint8_t* ResDesc_LoadFromFile(const resource_descriptor_t& descriptor, int* length_out);
bool ResDesc_NeedToDownload(const resource_descriptor_t& descriptor);
CRC32_t ResDesc_CalcFileCRC32(const resource_descriptor_t& descriptor);

std::unordered_map<std::string, resource_descriptor_t> ResDesc_GetAllOverwriteResources();
