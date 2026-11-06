#pragma once

#include <cstddef>
#include <string>

#include <ncl_material/MaterialDoc.h>

namespace render
{
    enum class MaterialReadStatus
    {
        Ready,
        NotFound,
        TooLarge,
        ReadFailed
    };

    class MaterialFileBackend
    {
    public:
        virtual ~MaterialFileBackend() = default;
        virtual MaterialReadStatus ReadFile(const char* path, std::string& text, size_t max_bytes) = 0;
        virtual bool Write(const std::string& relative, const std::string& text, std::string& error) = 0;
        virtual void Report(const char* text, bool debug) = 0;

        bool Read(const char* path, std::string& text, size_t max_bytes)
        {
            return ReadFile(path, text, max_bytes) == MaterialReadStatus::Ready;
        }
    };

    class MaterialDocumentStore
    {
    private:
        MaterialFileBackend& files_;

    public:
        explicit MaterialDocumentStore(MaterialFileBackend& files);
        bool Read(const char* path, std::string& text, size_t max_bytes = 0);
        MaterialReadStatus ReadFile(const char* path, std::string& text, size_t max_bytes = 0);
        bool Write(const std::string& relative, const std::string& text, std::string& error);
        ncl_material::MaterialDoc LoadModel(const std::string& name);
        void Report(const char* format, ...);
        void Debug(const char* format, ...);
    };
} // namespace render
