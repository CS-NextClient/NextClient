#pragma once
#include <string>
#include <vector>
#include <format>

#include <crc.h>

#include "hlsdk.h"

class ResourceDescriptor
{
    std::string filename_{};
    std::string save_path_{};
    std::string download_path_{};
    CRC32_t server_crc32_{};
    int download_size_{};
    bool is_allow_override_resource_{};
    bool is_private_resource_{};
    bool is_client_only_{};

public:
    explicit ResourceDescriptor() = default;

    // Constructor for regular resource
    explicit ResourceDescriptor(const std::string& filename, int download_size = 0);

    // Constructor for private resource
    explicit ResourceDescriptor(const std::string& filename, const std::string& download_path, CRC32_t server_crc32, int download_size, bool client_only);

    bool SaveToFile(const char* data, int length) const;
    [[nodiscard]] bool NeedToDownload() const;

    // Calculates the crc32 for the file get_save_path(). If the file does not exist or cannot be read, returns 0.
    [[nodiscard]] CRC32_t CalcSaveFileCRC32() const;
    [[nodiscard]] const std::string& get_filename() const { return filename_; }
    [[nodiscard]] const std::string& get_save_path() const { return save_path_; }
    [[nodiscard]] const std::string& get_download_path() const { return download_path_; }
    [[nodiscard]] CRC32_t get_server_crc32() const { return server_crc32_; }
    [[nodiscard]] int get_download_size() const { return download_size_; }
    [[nodiscard]] bool is_allow_override_resource() const { return is_allow_override_resource_; }
    [[nodiscard]] bool is_private_resource() const { return is_private_resource_; }
    [[nodiscard]] bool is_client_only() const { return is_client_only_; }

private:
    static bool IsAllowOverrideResource(const std::string& filename);
    static std::string GetDownloadsFolder();
    static std::string GetDownloadsPrivateFolder();
};


class ResourceDescriptorFactory
{
public:
    static ResourceDescriptor MakeByFilename(const std::string& filename);
    static ResourceDescriptor MakeByResourceT(const resource_t* resource);
    static std::vector<ResourceDescriptor> MakeByDownloadPath(const std::string& download_file_path);
};