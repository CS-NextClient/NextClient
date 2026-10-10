#include "plugin_file.h"
#include "runtime_budget.h"
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
    std::vector<unsigned char> File::Read(bool allow_empty, size_t limit) const
    {
        LARGE_INTEGER size{}, start{};
        if (!GetFileSizeEx(handle_, &size) || size.QuadPart < 0 || (!allow_empty && size.QuadPart == 0) ||
            static_cast<uint64_t>(size.QuadPart) > limit)
            throw std::runtime_error(message("#NextPlugins_ErrorDllSize"));
        runtime::Budget memory(static_cast<size_t>(size.QuadPart) * 2 + 256, true);
        std::vector<unsigned char> bytes(static_cast<size_t>(size.QuadPart));
        if (bytes.empty())
            return bytes;
        DWORD read{};
        if (!SetFilePointerEx(handle_, start, nullptr, FILE_BEGIN) ||
            !ReadFile(handle_, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) || read != bytes.size())
            throw std::runtime_error(message("#NextPlugins_ErrorReadCompleteDll"));
        return bytes;
    }
    std::string read_text(const fs::path& path, size_t limit)
    {
        if (!fs::exists(path))
            return "{}";
        if (fs::file_size(path) > limit)
            throw std::runtime_error(message("#NextPlugins_ErrorConfigSize"));
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            throw std::runtime_error(message("#NextPlugins_ErrorReadConfig"));
        std::string result;
        char buffer[4096];
        while (stream.read(buffer, sizeof(buffer)) || stream.gcount())
        {
            if (result.size() + static_cast<size_t>(stream.gcount()) > limit)
                throw std::runtime_error(message("#NextPlugins_ErrorConfigSize"));
            result.append(buffer, static_cast<size_t>(stream.gcount()));
        }
        if (!stream.eof())
            throw std::runtime_error(message("#NextPlugins_ErrorReadConfig"));
        return result;
    }
    void write_json(const fs::path& path, const Json& value)
    {
        // Use the reader's size, syntax and nesting checks before touching disk.
        const auto data = tao::json::to_string(value);
        parse(data);
        fs::create_directories(path.parent_path());
        auto temp = path;
        temp += L".tmp";
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
    void write_config(const fs::path& path, const Json& value, void (*validate)(const Json&))
    {
        validate(value);
        parse(tao::json::to_string(value));
        if (fs::exists(path))
        {
            // Only a readable, valid predecessor may replace the recovery copy.
            Json previous;
            bool valid = false;
            try
            {
                previous = parse(read_text(path));
                validate(previous);
                valid = true;
            }
            catch (const std::exception&)
            {
                auto damaged = path;
                damaged += L".damaged";
                fs::copy_file(path, damaged, fs::copy_options::overwrite_existing);
            }
            if (valid)
            {
                auto backup = path;
                backup += L".bak";
                write_json(backup, previous);
            }
        }
        write_json(path, value);
    }
} // namespace plugins
