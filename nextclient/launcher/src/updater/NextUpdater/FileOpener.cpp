#include "FileOpener.h"
#include <utils/platform.h>
#include <easylogging++.h>
#include <Windows.h>

using namespace ncl_utils;
namespace fs = std::filesystem;

OpenerFile::OpenerFile(std::filesystem::path filepath, std::fstream&& stream, std::string error, DWORD file_attributes_to_restore, OpenerFileFlags::Value flags) :
        filepath(std::move(filepath)),
        stream(std::move(stream)),
        error(std::move(error)),
        file_attributes_to_restore_(file_attributes_to_restore),
        flags(flags)
{ }

OpenerFile::OpenerFile(OpenerFile&& other) noexcept :
        filepath(std::move(other.filepath)),
        stream(std::move(other.stream)),
        error(std::move(other.error)),
        file_attributes_to_restore_(other.file_attributes_to_restore_),
        flags(other.flags)
{ }

OpenerFile::~OpenerFile()
{
    Close();
}

OpenerFile& OpenerFile::operator=(OpenerFile&& other) noexcept
{
    if (this == &other)
        return *this;

    filepath = std::move(other.filepath);
    stream = std::move(other.stream);
    error = std::move(other.error);
    file_attributes_to_restore_ = other.file_attributes_to_restore_;
    flags = other.flags;

    return *this;
}

void OpenerFile::Close()
{
    if (!stream.is_open())
        return;

    stream.close();

    std::error_code ec_is_empty;
    bool is_empty = fs::is_empty(filepath, ec_is_empty);

    if (file_attributes_to_restore_ != INVALID_FILE_ATTRIBUTES)
    {
        Result<> result = FileOpener::SetAttributes(filepath, file_attributes_to_restore_);
        if (result.has_error())
            LOG(ERROR) << "OpenerFile::Close | " << filepath.string() << ": can't restore attributes " << file_attributes_to_restore_ << ": " << result.error_str();
    }

    if (ec_is_empty)
        LOG(ERROR) << "OpenerFile::Close | " << filepath.string() << ": fs::is_empty error: " << ec_is_empty.message();

    if (is_empty && (flags & OpenerFileFlags::DeleteNewEmptyFileOnClose) && (flags & OpenerFileFlags::NewFile))
    {
        LOG(INFO) << "OpenerFile::Close | " << filepath.string() << ": deleting empty file";
        fs::remove(filepath);
    }
}

[[nodiscard]] bool OpenerFile::HasError(FileOpenerErrorFlags::Value error_flags) const
{
    bool shouldIgnoreNonExistentFile = !(flags & OpenerFileFlags::FileExists)
                                       && (error_flags & FileOpenerErrorFlags::IgnoreNotExists)
                                       && (error == kFileNotExistsStringError);

    return !shouldIgnoreNonExistentFile && !error.empty();
}

[[nodiscard]] bool OpenerFile::IsEmptyOrFullRead()
{
    return stream.peek() == std::ifstream::traits_type::eof();
}

[[nodiscard]] bool OpenerFile::IsFileExists() const
{
    return flags & OpenerFileFlags::FileExists;
}


FileOpener::FileOpener(std::ios_base::openmode openmode, bool delete_new_empty_files_on_close) :
    openmode_(openmode),
    delete_new_empty_files_on_close_(delete_new_empty_files_on_close)
{ }

FileOpener::~FileOpener()
{
    CloseAllFiles();
}

void FileOpener::CloseAllFiles()
{
    for (auto& [filename, file]: files_)
        file.Close();
}

bool FileOpener::IsAllFilesOpened(FileOpenerErrorFlags::Value flags)
{
    static std::string error_tmp;
    return IsAllFilesOpened(flags, error_tmp);
}

bool FileOpener::IsAllFilesOpened(FileOpenerErrorFlags::Value flags, std::string& error_out)
{
    error_out.clear();

    for (const auto& [filepath, file] : files_)
    {
        if (!file.HasError(flags))
            continue;

        error_out += CreateErrorMessage(file, filepath) + "\n";
    }

    return error_out.empty();
}

OpenerFile& FileOpener::GetFileInfo(const std::filesystem::path& filepath)
{
    return files_[filepath];
}

const OpenerFile& FileOpener::OpenFile(const std::filesystem::path& filepath)
{
    if (files_.contains(filepath))
        return files_[filepath];

    OpenerFile open_info;
    OpenSingleFileInternal(filepath, openmode_, delete_new_empty_files_on_close_, open_info);

    files_[filepath] = std::move(open_info);
    return files_[filepath];
}

OpenerFile FileOpener::OpenSingleFile(const std::filesystem::path& filepath, std::ios_base::openmode openmode, bool delete_new_empty_file_on_close)
{
    OpenerFile open_info;
    OpenSingleFileInternal(filepath, openmode, delete_new_empty_file_on_close, open_info);

    return std::move(open_info);
}

std::string FileOpener::CreateErrorMessage(const OpenerFile& open_info, const std::filesystem::path& filepath)
{
    std::ostringstream error_stream;

    error_stream << filepath.relative_path().string() << ": " << open_info.error;

    return error_stream.str();
}

std::string FileOpener::CreateErrorMessage(std::fstream& fstream, const std::filesystem::path& filepath)
{
    std::ostringstream errorStream;

    if (fstream.bad())
    {
        errorStream << filepath.relative_path().string() << ": system error: " << std::strerror(errno);
    }
    else if (fstream.fail())
    {
        errorStream << filepath.relative_path().string() << ": stream logic error (full read or empty: ";
        errorStream << (fstream.peek() == std::ifstream::traits_type::eof() ? "true" : "false") << ")";
    }

    return errorStream.str();
}

bool FileOpener::IsError(const std::fstream& fstream)
{
    return fstream.fail() || fstream.bad();
}

void FileOpener::OpenSingleFileInternal(const std::filesystem::path& filepath, std::ios_base::openmode openmode, bool delete_new_empty_file_on_close, OpenerFile& open_info)
{
    DWORD file_attributes_to_restore = INVALID_FILE_ATTRIBUTES;
    OpenerFileFlags::Value flags = OpenerFileFlags::None;

    std::error_code ec_exists;
    if (fs::exists(filepath, ec_exists))
        flags |= OpenerFileFlags::FileExists;

    if (delete_new_empty_file_on_close)
        flags |= OpenerFileFlags::DeleteNewEmptyFileOnClose;

    auto file_error = [filepath, flags, &open_info](const std::string& error) {
        open_info = std::move(OpenerFile(filepath, std::move(std::fstream()), error, INVALID_FILE_ATTRIBUTES, flags));
    };

    if (!(flags & OpenerFileFlags::FileExists) && ec_exists)
        return file_error("error on fs::exists: " + ec_exists.message());

    if ((openmode & std::ios::in) && !(openmode & std::ios::out))
    {
        if (!(flags & OpenerFileFlags::FileExists))
            return file_error(kFileNotExistsStringError);
    }

    if (openmode & std::ios::out)
    {
        if (!(flags & OpenerFileFlags::FileExists))
        {
            std::error_code ec_create_directories;
            if (filepath.has_parent_path() && !fs::create_directories(filepath.parent_path(), ec_create_directories) && ec_create_directories)
                return file_error("error on fs:create_directories at " + filepath.parent_path().string() + ": " + ec_create_directories.message());
        }
        else
        {
            auto remove_attr_result = RemoveWriteProtectAttributes(filepath);
            if (remove_attr_result.has_error())
                return file_error("can't remove write-protect attributes: " + remove_attr_result.error_str());

            file_attributes_to_restore = remove_attr_result->old_attributes;
        }
    }

    std::fstream file;

    errno = 0;
    file.open(filepath, openmode);
    if (!file.is_open())
    {
        std::string open_error = "error on open: " + std::string(strerror(errno));

        if (file_attributes_to_restore != INVALID_FILE_ATTRIBUTES)
        {
            Result restore_attrib_result = SetAttributes(filepath, file_attributes_to_restore);
            if (restore_attrib_result.has_error())
                open_error += std::format("; restore attributes ({}) error: {}", file_attributes_to_restore, restore_attrib_result.error_str());
        }

        return file_error(open_error);
    }

    if ((openmode & std::ios::out) && !(flags & OpenerFileFlags::FileExists))
        flags |= OpenerFileFlags::NewFile;

    open_info = std::move(OpenerFile(filepath, std::move(file), "", file_attributes_to_restore, flags));
}

ResultT<FileOpener::RemoveWriteProtectAttributesResult> FileOpener::RemoveWriteProtectAttributes(const std::filesystem::path& filepath)
{
    DWORD old_attributes = GetFileAttributesW(filepath.c_str());
    DWORD new_attributes = old_attributes;

    if (old_attributes == INVALID_FILE_ATTRIBUTES)
        return ResultError(GetWinErrorString(GetLastError()));

    if (old_attributes & FILE_ATTRIBUTE_HIDDEN)
        new_attributes &= ~FILE_ATTRIBUTE_HIDDEN;

    if (old_attributes & FILE_ATTRIBUTE_READONLY)
        new_attributes &= ~FILE_ATTRIBUTE_READONLY;

    if (old_attributes != new_attributes)
    {
        DWORD result = SetFileAttributesW(filepath.c_str(), new_attributes);
        if (result == 0)
            return ResultError(GetWinErrorString(GetLastError()));
    }

    return RemoveWriteProtectAttributesResult { old_attributes, new_attributes };
}

Result<> FileOpener::SetAttributes(const std::filesystem::path& filepath, DWORD attributes)
{
    DWORD result = SetFileAttributesW(filepath.c_str(), attributes);
    if (result == 0)
        return ResultError(GetWinErrorString(GetLastError()));

    return {};
}
