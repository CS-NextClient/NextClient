#include "graphics/render/material/MaterialGameFiles.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#else
#include <cstdlib>
#include <unistd.h>
#endif

#include "common/filesystem.h"
#include "console/console.h"

namespace render
{
    namespace
    {
        constexpr size_t kMaxReadBytes = static_cast<size_t>(std::numeric_limits<int>::max());

        bool CreateTemporaryOutput(const std::filesystem::path& destination, std::filesystem::path& temporary)
        {
#ifdef _WIN32
            wchar_t name[MAX_PATH]{};
            if (GetTempFileNameW(destination.parent_path().c_str(), L"ncm", 0, name) == 0)
            {
                return false;
            }

            temporary = name;
            return true;
#else
            std::string pattern = destination.string() + ".XXXXXX";
            std::vector<char> name(pattern.begin(), pattern.end());
            name.push_back('\0');
            const int descriptor = mkstemp(name.data());
            if (descriptor < 0)
            {
                return false;
            }

            temporary = name.data();
            if (close(descriptor) != 0)
            {
                std::error_code code;
                std::filesystem::remove(temporary, code);
                return false;
            }

            return true;
#endif
        }
    }

    void MaterialGameFiles::SetGameDirectory(const char* directory)
    {
        game_directory_ = directory;
    }

    MaterialReadStatus MaterialGameFiles::ReadFile(const char* path, std::string& text, size_t max_bytes)
    {
        FileHandle_t file = FS_Open(path, "rb");
        if (file == nullptr)
        {
            return FS_FileExists(path) ? MaterialReadStatus::ReadFailed : MaterialReadStatus::NotFound;
        }

        unsigned int size = FS_Size(file);
        if (size > kMaxReadBytes || (max_bytes != 0 && size > max_bytes))
        {
            const size_t limit = max_bytes != 0 && max_bytes < kMaxReadBytes ? max_bytes : kMaxReadBytes;
            Con_Printf("materials: %s is %u bytes, the limit is %u\n", path, size, static_cast<unsigned>(limit));
            FS_Close(file);
            return MaterialReadStatus::TooLarge;
        }

        text.resize(size);
        int read = size != 0 ? FS_Read(text.data(), static_cast<int>(size), file) : 0;
        FS_Close(file);
        return read == static_cast<int>(size) ? MaterialReadStatus::Ready : MaterialReadStatus::ReadFailed;
    }

    bool MaterialGameFiles::Write(const std::string& relative, const std::string& text, std::string& error)
    {
        if (game_directory_.empty())
        {
            error = "game directory is unavailable";
            return false;
        }

        std::filesystem::path path = std::filesystem::path(game_directory_) / relative;
        std::error_code code;
        std::filesystem::create_directories(path.parent_path(), code);
        if (code)
        {
            error = "cannot create directory " + path.parent_path().string();
            return false;
        }

        std::filesystem::path temporary;
        if (!CreateTemporaryOutput(path, temporary))
        {
            error = "cannot create temporary file for " + path.string();
            return false;
        }

        const auto fail = [&](const char* operation) {
            std::error_code remove_error;
            std::filesystem::remove(temporary, remove_error);
            error = std::string(operation) + path.string();
            return false;
        };
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            return fail("cannot open temporary file for ");
        }

        if (!WriteOutput(file, text))
        {
            file.close();
            return fail("cannot write ");
        }

        if (!CompleteOutput(file))
        {
            return fail("cannot complete ");
        }

        if (!ReplaceOutput(temporary, path))
        {
            return fail("cannot replace ");
        }

        return true;
    }

    bool MaterialGameFiles::WriteOutput(std::ofstream& file, const std::string& text)
    {
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(file);
    }

    bool MaterialGameFiles::CompleteOutput(std::ofstream& file)
    {
        file.flush();
        const bool flushed = static_cast<bool>(file);
        file.close();
        return flushed && static_cast<bool>(file);
    }

    bool MaterialGameFiles::ReplaceOutput(const std::filesystem::path& temporary, const std::filesystem::path& destination)
    {
#ifdef _WIN32
        return MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
        std::error_code code;
        std::filesystem::rename(temporary, destination, code);
        return !code;
#endif
    }

    void MaterialGameFiles::Report(const char* text, bool debug)
    {
        if (debug)
        {
            Con_DPrintf(ConLogType::Info, "%s", text);
        }
        else
        {
            Con_Printf("%s", text);
        }
    }
} // namespace render
