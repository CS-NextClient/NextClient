#pragma once

#include <cstdint>
#include <string>

namespace render::preview
{
    class PreviewAssetBackend
    {
    public:
        using LoadFile_t = uint8_t* (*)(const char*, int, int*);
        using FreeFile_t = void (*)(void*);

    private:
        LoadFile_t load_file_{};
        FreeFile_t free_file_{};

    public:
        PreviewAssetBackend() = default;
        PreviewAssetBackend(LoadFile_t load_file, FreeFile_t free_file);
        void BindFiles(LoadFile_t load_file, FreeFile_t free_file);
        bool is_available() const;
        uint8_t* LoadFile(const char* path, int* length);
        void FreeFile(void* bytes);
        std::string LoadText(const char* path);
        int UploadMiptex(const char* identifier, uint8_t* lump, int lump_size);
        void UnloadTexture(const char* identifier);
        void Report(const char* format, ...);
    };
} // namespace render::preview
