#include "graphics/render/material/MaterialDocumentStore.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

#include <ncl_material/material_parse.h>

namespace render
{
    namespace
    {
        std::string Format(const char* format, va_list args)
        {
            va_list copy;
            va_copy(copy, args);
            int length = std::vsnprintf(nullptr, 0, format, copy);
            va_end(copy);
            if (length < 0)
            {
                return {};
            }

            std::vector<char> text(static_cast<size_t>(length) + 1);
            std::vsnprintf(text.data(), text.size(), format, args);
            return std::string(text.data(), static_cast<size_t>(length));
        }
    } // namespace

    MaterialDocumentStore::MaterialDocumentStore(MaterialFileBackend& files) :
        files_(files)
    {}

    bool MaterialDocumentStore::Read(const char* path, std::string& text, size_t max_bytes)
    {
        return files_.Read(path, text, max_bytes);
    }

    MaterialReadStatus MaterialDocumentStore::ReadFile(const char* path, std::string& text, size_t max_bytes)
    {
        return files_.ReadFile(path, text, max_bytes);
    }

    bool MaterialDocumentStore::Write(const std::string& relative, const std::string& text, std::string& error)
    {
        return files_.Write(relative, text, error);
    }

    ncl_material::MaterialDoc MaterialDocumentStore::LoadModel(const std::string& name)
    {
        if (name.size() < 4)
        {
            return {};
        }

        std::string path = name.substr(0, name.size() - 4) + ".mat";
        std::string text;
        if (!Read(path.c_str(), text))
        {
            return {};
        }

        ncl_utils::ResultT<ncl_material::MaterialDoc> parsed = ncl_material::ParseMaterialDoc(text);
        if (parsed.has_error())
        {
            Report("render: %s: %s\n", path.c_str(), parsed.error_str().c_str());
            return {};
        }

        Debug("render: %s: %u materials\n", path.c_str(), static_cast<unsigned>(parsed->entries.size()));
        return *parsed;
    }

    void MaterialDocumentStore::Report(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        std::string text = Format(format, args);
        va_end(args);
        files_.Report(text.c_str(), false);
    }

    void MaterialDocumentStore::Debug(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        std::string text = Format(format, args);
        va_end(args);
        files_.Report(text.c_str(), true);
    }
} // namespace render
