#pragma once

#include <iostream>
#include <fstream>
#include <ranges>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <Windows.h>
#include <ncl_utils/safe_result.h>
#include <utils/bitmask.h>

inline constexpr auto kFileNotExistsStringError = "file is not exists";

namespace FileOpenerErrorFlags
{
    enum Value : uint8_t
    {
        None            = 0,
        IgnoreNotExists = 1 << 0
    };

    BITMASK_OPS(Value)
}

namespace OpenerFileFlags
{
    enum Value : uint8_t
    {
        None        = 0,
        FileExists  = 1 << 0,
        NewFile     = 1 << 1,
        DeleteNewEmptyFileOnClose = 1 << 2
    };

    BITMASK_OPS(Value)
}

class OpenerFile
{
    DWORD file_attributes_to_restore_ = INVALID_FILE_ATTRIBUTES;

public:
    std::filesystem::path filepath;
    std::fstream stream;
    std::string error;
    OpenerFileFlags::Value flags{};

    explicit OpenerFile() = default;
    explicit OpenerFile(std::filesystem::path filepath, std::fstream&& stream, std::string error, DWORD file_attributes_to_restore, OpenerFileFlags::Value flags);
    ~OpenerFile();

    OpenerFile(OpenerFile&& other) noexcept;
    OpenerFile& operator=(OpenerFile&& other) noexcept;

    void Close();
    [[nodiscard]] bool HasError(FileOpenerErrorFlags::Value error_flags = FileOpenerErrorFlags::None) const;

    // Checks if the file is empty or has been read to the end.
    //
    // If an error occurs, it will return true. Use HasError beforehand if necessary.
    [[nodiscard]] bool IsEmptyOrFullRead();
    [[nodiscard]] bool IsFileExists() const;
};


class FileOpener
{
    friend OpenerFile;

    struct RemoveWriteProtectAttributesResult
    {
        DWORD old_attributes;
        DWORD new_attributes;
    };

    std::ios_base::openmode openmode_;
    bool delete_new_empty_files_on_close_;
    std::unordered_map<std::filesystem::path, OpenerFile> files_;

public:
    explicit FileOpener(std::ios_base::openmode openmode, bool delete_new_empty_files_on_close = true);
    explicit FileOpener(auto files, std::ios_base::openmode openmode, bool delete_new_empty_files_on_close = true) :
        openmode_(openmode),
        delete_new_empty_files_on_close_(delete_new_empty_files_on_close)
    {
        OpenFiles(files);
    }
    ~FileOpener();

    void CloseAllFiles();
    [[nodiscard]] bool IsAllFilesOpened(FileOpenerErrorFlags::Value flags);
    [[nodiscard]] bool IsAllFilesOpened(FileOpenerErrorFlags::Value flags, std::string& error_out);
    [[nodiscard]] OpenerFile& GetFileInfo(const std::filesystem::path& filepath);
    const OpenerFile& OpenFile(const std::filesystem::path& filepath);
    void OpenFiles(auto files)
    {
        for (const auto& file : files)
            OpenFile(file);
    }

    [[nodiscard]] static OpenerFile OpenSingleFile(const std::filesystem::path& filepath, std::ios_base::openmode openmode, bool delete_new_empty_file_on_close = true);
    [[nodiscard]] static std::string CreateErrorMessage(const OpenerFile& open_info, const std::filesystem::path& filepath);
    [[nodiscard]] static std::string CreateErrorMessage(std::fstream& fstream, const std::filesystem::path& filepath);
    [[nodiscard]] static bool IsError(const std::fstream& fstream);

private:
    static void OpenSingleFileInternal(const std::filesystem::path& filepath, std::ios_base::openmode openmode, bool delete_new_empty_file_on_close, OpenerFile& open_info);
    static ncl_utils::ResultT<RemoveWriteProtectAttributesResult> RemoveWriteProtectAttributes(const std::filesystem::path& filepath);
    static ncl_utils::Result<> SetAttributes(const std::filesystem::path& filepath, DWORD attributes);
};