#include "plugin_file.h"
#include <fstream>

namespace plugins
{
    namespace fs = std::filesystem;
    File::File(const fs::path& path)
    {
        // Deny writes/deletes, including during hash validation and loading.
        handle_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE)
            throw std::runtime_error(message("#NextPlugins_ErrorReadDll"));
        wchar_t resolved[32768]{};
        auto length = GetFinalPathNameByHandleW(handle_, resolved, 32768, FILE_NAME_NORMALIZED);
        if (!length || length >= 32768)
        {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
            throw std::runtime_error(message("#NextPlugins_ErrorResolvePath"));
        }
        try
        {
            resolved_path_.assign(resolved, length);
        }
        catch (...)
        {
            CloseHandle(handle_);
            throw;
        }
    }
    File::~File()
    {
        if (handle_ != INVALID_HANDLE_VALUE)
            CloseHandle(handle_);
    }
    std::vector<unsigned char> File::Read(bool allow_empty) const
    {
        LARGE_INTEGER size{}, start{};
        if (!GetFileSizeEx(handle_, &size) || size.QuadPart < 0 || (!allow_empty && size.QuadPart == 0) || size.QuadPart > 64 * 1024 * 1024)
            throw std::runtime_error(message("#NextPlugins_ErrorDllSize"));
        std::vector<unsigned char> bytes(static_cast<size_t>(size.QuadPart));
        if (bytes.empty())
            return bytes;
        DWORD read{};
        if (!SetFilePointerEx(handle_, start, nullptr, FILE_BEGIN) ||
            !ReadFile(handle_, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) || read != bytes.size())
            throw std::runtime_error(message("#NextPlugins_ErrorReadCompleteDll"));
        return bytes;
    }
    std::string read_text(const fs::path& path)
    {
        if (!fs::exists(path))
            return "{}";
        if (fs::file_size(path) > 1024 * 1024)
            throw std::runtime_error(message("#NextPlugins_ErrorConfigSize"));
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            throw std::runtime_error(message("#NextPlugins_ErrorReadConfig"));
        return {std::istreambuf_iterator<char>(stream), {}};
    }
    void write_json(const fs::path& path, const Json& value)
    {
        fs::create_directories(path.parent_path());
        auto temp = path;
        temp += L".tmp";
        auto data = tao::json::to_string(value);
        HANDLE h = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            throw std::runtime_error(message("#NextPlugins_ErrorSaveConfig"));
        DWORD written{};
        bool ok =
            WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) && written == data.size() && FlushFileBuffers(h);
        CloseHandle(h);
        if (!ok || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error(message("#NextPlugins_ErrorCommitConfig"));
    }
} // namespace plugins
