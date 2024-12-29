#include "ResourceDescriptor.h"

#include <engine.h>
#include <client/cl_private_resources.h>
#include <common/com_strings.h>
#include <common/filesystem.h>
#include <console/console.h>
#include <nitro_utils/string_utils.h>

ResourceDescriptor::ResourceDescriptor(const std::string& filename, int download_size):
    filename_(filename),
    save_path_(std::format("{}/{}", GetDownloadsFolder(), filename)),
    download_path_(filename),
    download_size_(download_size)
{
}

ResourceDescriptor::ResourceDescriptor(const std::string& filename, const std::string& download_path, CRC32_t server_crc32, int download_size, bool client_only):
    filename_(filename),
    save_path_(std::format("{}/{}", GetDownloadsPrivateFolder(), filename)),
    download_path_(download_path),
    server_crc32_(server_crc32),
    download_size_(download_size),
    is_allow_override_resource_(IsAllowOverrideResource(filename)),
    is_private_resource_(true),
    is_client_only_(client_only)
{
}

bool ResourceDescriptor::SaveToFile(const char* data, int length) const
{
    std::filesystem::path full_path = save_path_;

    if (full_path.has_parent_path())
    {
        std::filesystem::create_directories(full_path.parent_path());
    }

    std::ofstream file(full_path, std::ios::out | std::ios::binary);
    if (!file.is_open())
    {
        Con_DPrintf(ConLogType::Error, "%s: Can't open '%s' to save '%s'\n", __func__, save_path_.c_str(), filename_.c_str());
        return false;
    }

    if (!file.write(data, length))
    {
        Con_DPrintf(ConLogType::Error, "%s: Can't write data to '%s' for '%s'\n", __func__, save_path_.c_str(), filename_.c_str());
        return false;
    }

    return true;
}

bool ResourceDescriptor::NeedToDownload() const
{
    if (!is_private_resource_)
    {
        return !FS_FileExists(filename_.c_str());
    }

    if (std::filesystem::exists(save_path_))
    {
        return server_crc32_ != CalcSaveFileCRC32();
    }

    if (is_allow_override_resource_)
    {
        return true;
    }

    return !FS_FileExists(filename_.c_str());
}

CRC32_t ResourceDescriptor::CalcSaveFileCRC32() const
{
    std::ifstream file(save_path_, std::ifstream::in | std::ifstream::ate | std::ifstream::binary);
    if (!file.is_open())
    {
        return 0;
    }
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> file_buffer(file_size);

    if (!file.read(file_buffer.data(), file_size))
    {
        return 0;
    }
    file.close();

    CRC32_t crc32;
    g_engfuncs.pfnCRC32_Init(&crc32);
    g_engfuncs.pfnCRC32_ProcessBuffer(&crc32, file_buffer.data(), file_size);
    crc32 = g_engfuncs.pfnCRC32_Final(crc32);

    return crc32;
}

bool ResourceDescriptor::IsAllowOverrideResource(const std::string& filename)
{
    std::string file_path_lower = nitro_utils::to_lower_copy(filename);

    bool allow_override = !file_path_lower.ends_with(".spr");

    return allow_override;
}

std::string ResourceDescriptor::GetDownloadsFolder()
{
    return std::format("{}_downloads", gEngfuncs.pfnGetGameDirectory());
}

std::string ResourceDescriptor::GetDownloadsPrivateFolder()
{
    return std::format("{}_downloads_private/{}", gEngfuncs.pfnGetGameDirectory(), PrivateRes_GetPrivateFolder());
}

ResourceDescriptor ResourceDescriptorFactory::MakeByFilename(const std::string& filename)
{
    int download_size = 0;
    if (client_stateex.resourcesNeeded.contains(filename))
    {
        download_size = client_stateex.resourcesNeeded.at(filename).nDownloadSize;
    }

    if (client_stateex.privateResources.contains(filename))
    {
        return client_stateex.privateResources.at(filename);
    }

    return ResourceDescriptor(filename, download_size);
}

ResourceDescriptor ResourceDescriptorFactory::MakeByResourceT(const resource_t* resource)
{
    if (resource->type == t_sound)
    {
        return MakeByFilename(va("%s%s", DEFAULT_SOUNDPATH, resource->szFileName));
    }

    return MakeByFilename(resource->szFileName);
}

std::vector<ResourceDescriptor> ResourceDescriptorFactory::MakeByDownloadPath(const std::string& download_file_path)
{
    std::vector<ResourceDescriptor> descriptors;

    auto it = client_stateex.privateResourcesReverseCache.find(download_file_path);
    if (it != client_stateex.privateResourcesReverseCache.end())
    {
        for (const auto& file_path : it->second)
        {
            descriptors.emplace_back(client_stateex.privateResources.at(file_path));
        }
    }
    else
    {
        descriptors.emplace_back(download_file_path);
    }

    return descriptors;
}
