#pragma once
#include "catalog.h"
#include <windows.h>

namespace plugins
{
    // Keeps the exact approved file locked while its module is loaded. Read()
    // owns its bytes separately so the lock never retains an image-sized buffer.
    class File
    {
    public:
        explicit File(const std::filesystem::path& path);
        ~File();
        File(const File&) = delete;
        File& operator=(const File&) = delete;
        std::vector<unsigned char> Read(bool allow_empty = false) const;
        const std::wstring& ResolvedPath() const
        {
            return resolved_path_;
        }

    private:
        HANDLE handle_{INVALID_HANDLE_VALUE};
        std::wstring resolved_path_;
    };
    std::string read_text(const std::filesystem::path& path);
    void write_json(const std::filesystem::path& path, const Json& value);
} // namespace plugins
