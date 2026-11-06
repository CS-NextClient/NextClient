#pragma once

#include <optional>
#include <string>
#include <vector>

class IFileSystem;

namespace material_editor
{
    struct ResourceEntry
    {
        std::string name;
        bool directory{};
    };

    struct ModelHeader
    {
        int id{};
        int body_parts{};
    };

    class ResourceSource
    {
    public:
        virtual ~ResourceSource() = default;

        // Preserves encounter order and duplicates, with copied names and no open search on return.
        virtual std::vector<ResourceEntry> Enumerate(const std::string& wildcard) = 0;
        // Only a complete studio header produces a value.
        virtual std::optional<ModelHeader> ReadModelHeader(const std::string& path) = 0;
        virtual bool FileExists(const std::string& path) = 0;
    };

    class FileSystemResourceSource : public ResourceSource
    {
    private:
        IFileSystem& filesystem_;

    public:
        explicit FileSystemResourceSource(IFileSystem& filesystem);
        std::vector<ResourceEntry> Enumerate(const std::string& wildcard) override;
        std::optional<ModelHeader> ReadModelHeader(const std::string& path) override;
        bool FileExists(const std::string& path) override;
    };

    class ResourceCatalog
    {
    private:
        ResourceSource& source_;

    public:
        explicit ResourceCatalog(ResourceSource& source);
        std::vector<std::string> ScanModels();
        std::vector<std::string> ScanImageFolders();
        std::vector<std::string> ScanImages(const std::string& folder);
        std::vector<std::string> GetStandInModels(const std::string& model);
        std::vector<std::string> GetRelatedModels(const std::string& model);

    private:
        void CollectFiles(const std::string& folder, const char* pattern, std::vector<std::string>& out);
        void CollectFolders(const std::string& folder, int depth, std::vector<std::string>& out);
        void CollectModels(const std::string& folder, int depth, std::vector<std::string>& out);
        bool CheckModelPart(const std::string& path);
        bool CheckPartOfAnotherModel(const std::vector<std::string>& sorted, const std::string& path);
    };
} // namespace material_editor
