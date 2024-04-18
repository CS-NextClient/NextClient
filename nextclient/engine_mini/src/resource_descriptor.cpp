#include <filesystem>
#include <fstream>
#include <nitro_utils/string_utils.h>
#include "resource_descriptor.h"
#include "console/console.h"
#include "common/zone.h"
#include "common/filesystem.h"
#include "common/sys_dll.h"
#include "common/com_strings.h"
#include "client/cl_private_resources.h"

static std::string GetDownloadsFolder()
{
    return std::format("{}_downloads", gEngfuncs.pfnGetGameDirectory());
}

static std::string GetDownloadsPrivateFolder()
{
    return std::format("{}_downloads_private/{}", gEngfuncs.pfnGetGameDirectory(), PrivateRes_GetPrivateFolder());
}

resource_descriptor_t ResDesc_Make(const std::string& file_path)
{
    resource_descriptor_t desc;

    if (client_stateex.resourcesNeeded.contains(file_path))
        desc.download_size = client_stateex.resourcesNeeded[file_path].nDownloadSize;

    if (client_stateex.privateResources.contains(file_path))
    {
        PrivateResInternal &res_private = client_stateex.privateResources[file_path];

        desc.filename = file_path;
        desc.save_path = std::format("{}/{}", GetDownloadsPrivateFolder(), file_path);
        desc.download_path = res_private.download_file_path;
        desc.server_crc32 = res_private.server_crc32;
        desc.private_resource = true;
        desc.only_client = res_private.only_client;
    }
    else
    {
        desc.filename = file_path;
        desc.save_path = std::format("{}/{}", GetDownloadsFolder(), file_path);
        desc.download_path = file_path;
        desc.server_crc32 = 0;
        desc.private_resource = false;
        desc.only_client = false;
    }

    return desc;
}

resource_descriptor_t ResDesc_Make(resource_t* resource)
{
    resource_descriptor_t desc;

    if (resource->type == t_sound)
        desc = ResDesc_Make(va("%s%s", DEFAULT_SOUNDPATH, resource->szFileName));
    else
        desc = ResDesc_Make(resource->szFileName);

    return desc;
}

std::vector<resource_descriptor_t> ResDesc_MakeByDownloadPath(const std::string& download_file_path)
{
    std::vector<resource_descriptor_t> descs;

    if (!client_stateex.privateResourcesReverseCache.contains(download_file_path))
    {
        descs.emplace_back(ResDesc_Make(download_file_path));
    }
    else
    {
        for (const auto& file_path : client_stateex.privateResourcesReverseCache[download_file_path])
            descs.emplace_back(ResDesc_Make(file_path));
    }

    return descs;
}

bool ResDesc_SaveToFile(const resource_descriptor_t& descriptor, const char* data, int length)
{
    std::filesystem::path full_path = descriptor.save_path;

    if (full_path.has_parent_path())
        std::filesystem::create_directories(full_path.parent_path());

    std::ofstream file(full_path, std::ios::out | std::ios::binary);
    if (!file.is_open())
        return false;

    if (!file.write((const char*)data, length))
        return false;

    return true;
}

bool ResDesc_NeedToDownload(const resource_descriptor_t& descriptor)
{
    if (std::filesystem::exists(descriptor.save_path) || FS_FileExists(descriptor.filename.c_str()))
    {
        if (!descriptor.private_resource)
            return false;

        return descriptor.server_crc32 != ResDesc_CalcFileCRC32(descriptor);
    }

    return true;
}

CRC32_t ResDesc_CalcFileCRC32(const resource_descriptor_t& descriptor)
{
    std::ifstream file(descriptor.save_path, std::ifstream::in | std::ifstream::ate | std::ifstream::binary);
    if (!file.is_open())
    {
        Con_DPrintf(ConLogType::Info, "Can't open file \"%s\" to check crc\n", descriptor.save_path.c_str());
        return true;
    }
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> file_buffer(file_size);

    if (!file.read(file_buffer.data(), file_size))
    {
        Con_Printf("Can't read file \"%s\" to check crc\n", descriptor.save_path.c_str());
        return true;
    }
    file.close();

    CRC32_t crc32;
    g_engfuncs.pfnCRC32_Init(&crc32);
    g_engfuncs.pfnCRC32_ProcessBuffer(&crc32, file_buffer.data(), file_size);
    crc32 = g_engfuncs.pfnCRC32_Final(crc32);

    return crc32;
}

std::unordered_map<std::string, resource_descriptor_t> ResDesc_GetAllOverwriteResources()
{
    std::unordered_map<std::string, resource_descriptor_t> result;
    result.reserve(client_stateex.privateResources.size());

    for (const auto &[path, res_private]: client_stateex.privateResources)
        result[path] = ResDesc_Make(path);

    return result;
}
