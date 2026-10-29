#include "GameUi/MaterialEditor/MaterialResourceCatalog.h"

#include <algorithm>

#include <FileSystem.h>
#include <goldsrc/format/studio.h>
#include <tier1/strtools.h>

namespace material_editor
{
    namespace
    {
        constexpr const char* kPrefixes[] = {"v_", "p_", "w_"};
        constexpr const char* kCollectionRoot = "materials";
        constexpr int kCollectionDepth = 2;
        constexpr const char* kCatalogRoot = "models";
        constexpr int kCatalogDepth = 3;
        // Raw search-path duplicates consume this limit before deduplication and companion filtering.
        constexpr size_t kMaxCatalogModels = 16384;
        constexpr int kStudioModelId = ('T' << 24) | ('S' << 16) | ('D' << 8) | 'I';

        bool IsImage(const char* name)
        {
            const char* dot = V_strrchr(name, '.');

            return dot != nullptr && (V_stricmp(dot, ".png") == 0 || V_stricmp(dot, ".tga") == 0);
        }

        bool IsModelFile(const char* name)
        {
            const char* dot = V_strrchr(name, '.');

            return dot != nullptr && V_stricmp(dot, ".mdl") == 0;
        }

        void AddUnique(std::vector<std::string>& out, const std::string& value)
        {
            for (const std::string& existing : out)
            {
                if (V_stricmp(existing.c_str(), value.c_str()) == 0)
                {
                    return;
                }
            }

            out.push_back(value);
        }

        bool LessIgnoreCase(const std::string& left, const std::string& right)
        {
            return V_stricmp(left.c_str(), right.c_str()) < 0;
        }

        bool IsDigit(char c)
        {
            return c >= '0' && c <= '9';
        }

        std::vector<std::string> CollectSiblings(const std::string& model)
        {
            size_t slash = model.find_last_of('/');
            std::string directory = slash != std::string::npos ? model.substr(0, slash + 1) : std::string();
            std::string file = slash != std::string::npos ? model.substr(slash + 1) : model;
            std::string stem = file;

            for (const char* prefix : kPrefixes)
            {
                if (file.rfind(prefix, 0) == 0)
                {
                    stem = file.substr(2);
                    break;
                }
            }

            if (stem == file)
            {
                return {model};
            }

            std::vector<std::string> siblings;

            for (const char* prefix : kPrefixes)
            {
                siblings.push_back(directory + prefix + stem);
            }

            return siblings;
        }
    } // namespace

    FileSystemResourceSource::FileSystemResourceSource(IFileSystem& filesystem) :
        filesystem_(filesystem)
    {}

    std::vector<ResourceEntry> FileSystemResourceSource::Enumerate(const std::string& wildcard)
    {
        FileFindHandle_t handle{};
        const char* name = filesystem_.FindFirst(wildcard.c_str(), &handle);

        if (name == nullptr)
        {
            return {};
        }

        std::vector<ResourceEntry> entries;

        while (name != nullptr)
        {
            entries.push_back({name, filesystem_.FindIsDirectory(handle)});
            name = filesystem_.FindNext(handle);
        }

        filesystem_.FindClose(handle);

        return entries;
    }

    std::optional<ModelHeader> FileSystemResourceSource::ReadModelHeader(const std::string& path)
    {
        FileHandle_t file = filesystem_.Open(path.c_str(), "rb");

        if (file == nullptr)
        {
            return std::nullopt;
        }

        goldsrc::studiohdr_t header{};
        int read = filesystem_.Read(&header, sizeof(header), file);

        filesystem_.Close(file);

        if (read != static_cast<int>(sizeof(header)))
        {
            return std::nullopt;
        }

        return ModelHeader{header.id, header.numbodyparts};
    }

    bool FileSystemResourceSource::FileExists(const std::string& path)
    {
        return filesystem_.FileExists(path.c_str());
    }

    ResourceCatalog::ResourceCatalog(ResourceSource& source) :
        source_(source)
    {}

    std::vector<std::string> ResourceCatalog::ScanModels()
    {
        std::vector<std::string> found;
        CollectModels(kCatalogRoot, kCatalogDepth, found);

        std::sort(found.begin(), found.end(), LessIgnoreCase);
        found.erase(
            std::unique(
                found.begin(),
                found.end(),
                [](const std::string& left, const std::string& right) { return V_stricmp(left.c_str(), right.c_str()) == 0; }
            ),
            found.end()
        );

        std::vector<std::string> models;

        // Companion lookup needs the full sorted list, including paths excluded from the result.
        for (const std::string& path : found)
        {
            if (!CheckPartOfAnotherModel(found, path))
            {
                models.push_back(path);
            }
        }

        return models;
    }

    std::vector<std::string> ResourceCatalog::ScanImageFolders()
    {
        std::vector<std::string> folders;
        CollectFolders(kCollectionRoot, kCollectionDepth, folders);
        std::sort(folders.begin(), folders.end());

        return folders;
    }

    std::vector<std::string> ResourceCatalog::ScanImages(const std::string& folder)
    {
        std::vector<std::string> images;
        CollectFiles(folder, "*.png", images);
        CollectFiles(folder, "*.tga", images);
        std::sort(images.begin(), images.end());

        return images;
    }

    std::vector<std::string> ResourceCatalog::GetStandInModels(const std::string& model)
    {
        std::vector<std::string> models = CollectSiblings(model);
        std::erase_if(models, [this](const std::string& path) { return !source_.FileExists(path); });

        if (!model.empty())
        {
            AddUnique(models, model);
        }

        return models;
    }

    std::vector<std::string> ResourceCatalog::GetRelatedModels(const std::string& model)
    {
        std::vector<std::string> related;

        for (const std::string& path : CollectSiblings(model))
        {
            if (path != model && source_.FileExists(path))
            {
                related.push_back(path);
            }
        }

        return related;
    }

    void ResourceCatalog::CollectFiles(const std::string& folder, const char* pattern, std::vector<std::string>& out)
    {
        for (const ResourceEntry& entry : source_.Enumerate(folder + "/" + pattern))
        {
            if (!entry.directory && IsImage(entry.name.c_str()))
            {
                AddUnique(out, folder + "/" + entry.name);
            }
        }
    }

    void ResourceCatalog::CollectFolders(const std::string& folder, int depth, std::vector<std::string>& out)
    {
        std::vector<std::string> images;
        CollectFiles(folder, "*.png", images);
        CollectFiles(folder, "*.tga", images);

        if (!images.empty())
        {
            AddUnique(out, folder);
        }

        if (depth <= 0)
        {
            return;
        }

        std::vector<std::string> children;

        for (const ResourceEntry& entry : source_.Enumerate(folder + "/*"))
        {
            if (entry.directory && entry.name != "." && entry.name != "..")
            {
                AddUnique(children, entry.name);
            }
        }

        for (const std::string& child : children)
        {
            CollectFolders(folder + "/" + child, depth - 1, out);
        }
    }

    void ResourceCatalog::CollectModels(const std::string& folder, int depth, std::vector<std::string>& out)
    {
        std::vector<std::string> children;

        for (const ResourceEntry& entry : source_.Enumerate(folder + "/*"))
        {
            if (entry.directory)
            {
                if (entry.name != "." && entry.name != "..")
                {
                    AddUnique(children, entry.name);
                }
            }
            else if (IsModelFile(entry.name.c_str()) && out.size() < kMaxCatalogModels)
            {
                out.push_back(folder + "/" + entry.name);
            }
        }

        if (depth <= 0)
        {
            return;
        }

        for (const std::string& child : children)
        {
            CollectModels(folder + "/" + child, depth - 1, out);
        }
    }

    bool ResourceCatalog::CheckModelPart(const std::string& path)
    {
        const std::optional<ModelHeader> header = source_.ReadModelHeader(path);

        return header.has_value() && (header->id != kStudioModelId || header->body_parts <= 0);
    }

    // studiomdl names texture companions <stem>T.mdl and sequence groups <stem>NN.mdl.
    bool ResourceCatalog::CheckPartOfAnotherModel(const std::vector<std::string>& sorted, const std::string& path)
    {
        if (path.size() < 5)
        {
            return false;
        }

        size_t stem = path.size() - 4;
        std::string base;

        if (path[stem - 1] == 'T' || path[stem - 1] == 't')
        {
            base = path.substr(0, stem - 1) + ".mdl";
        }
        else if (stem >= 2 && IsDigit(path[stem - 1]) && IsDigit(path[stem - 2]))
        {
            base = path.substr(0, stem - 2) + ".mdl";
        }
        else
        {
            return false;
        }

        return std::binary_search(sorted.begin(), sorted.end(), base, LessIgnoreCase) && CheckModelPart(path);
    }
} // namespace material_editor
