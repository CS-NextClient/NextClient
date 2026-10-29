#pragma once

#include <filesystem>
#include <fstream>
#include <string>

#include "graphics/render/material/MaterialDocumentStore.h"

namespace render
{
    class MaterialGameFiles : public MaterialFileBackend
    {
    private:
        std::string game_directory_;

    public:
        void SetGameDirectory(const char* directory);
        MaterialReadStatus ReadFile(const char* path, std::string& text, size_t max_bytes) override;
        bool Write(const std::string& relative, const std::string& text, std::string& error) override;
        void Report(const char* text, bool debug) override;

    protected:
        virtual bool WriteOutput(std::ofstream& file, const std::string& text);
        virtual bool CompleteOutput(std::ofstream& file);
        virtual bool ReplaceOutput(const std::filesystem::path& temporary, const std::filesystem::path& destination);
    };
} // namespace render
