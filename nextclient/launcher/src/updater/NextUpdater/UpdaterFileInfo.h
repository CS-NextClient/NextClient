#pragma once

#include <filesystem>
#include <updater/json_data/FileEntry.h>

class UpdaterFileInfo
{
    std::filesystem::path to_hash_path_;
    std::filesystem::path install_path_;
    std::filesystem::path backup_path_;
    FileEntry remote_file_;
    bool need_backup_;

public:
    explicit UpdaterFileInfo(std::filesystem::path to_hash_path, std::filesystem::path install_path, std::filesystem::path backup_path, FileEntry remote_file, bool need_backup) :
            to_hash_path_(std::move(to_hash_path)),
            install_path_(std::move(install_path)),
            backup_path_(std::move(backup_path)),
            remote_file_(std::move(remote_file)),
            need_backup_(need_backup)
    { }

    [[nodiscard]] const std::filesystem::path& get_to_hash_path() const { return to_hash_path_; }
    [[nodiscard]] const std::filesystem::path& get_install_path() const { return install_path_; }
    [[nodiscard]] const std::filesystem::path& get_backup_path() const { return backup_path_; }
    [[nodiscard]] const FileEntry& get_remote_file() const { return remote_file_; }
    [[nodiscard]] bool is_need_backup() const { return need_backup_; }
};