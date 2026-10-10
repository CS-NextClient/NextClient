#pragma once
#include "plugin_file.h"
#include <memory>

namespace plugins
{
    struct Package
    {
        std::string hash;
        Manifest manifest;
        std::vector<std::unique_ptr<File>> files;
        std::vector<size_t> binaries;
        size_t entry{};
        std::vector<HMODULE> modules;
        std::vector<HANDLE> directories;
        ~Package();
        HMODULE Load();
    };
    std::unique_ptr<Package> read_package(const std::filesystem::path& path);
} // namespace plugins
