#include "plugin_package.h"
#include "pe_image.h"
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <cstring>

namespace plugins
{
    namespace fs = std::filesystem;
    namespace
    {
        std::wstring lower(std::wstring name)
        {
            std::transform(name.begin(), name.end(), name.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
            return name;
        }
        std::string filename_utf8(const fs::path& path)
        {
            const auto name = path.filename().u8string();
            return {name.begin(), name.end()};
        }
        std::vector<std::wstring> imports(const std::vector<unsigned char>& bytes)
        {
            const PeImage image(bytes);
            if (image.optional.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].Size)
                throw std::runtime_error(message("#NextPlugins_ErrorDelayImports"));
            std::vector<std::wstring> result;
            const auto table = image.optional.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            if (!table.VirtualAddress)
                return result;
            for (size_t n = 0; n < 512; ++n)
            {
                const auto row =
                    image.Read<IMAGE_IMPORT_DESCRIPTOR>(image.Offset(table.VirtualAddress) + n * sizeof(IMAGE_IMPORT_DESCRIPTOR));
                if (!row.Name)
                    return result;
                auto start = image.Offset(row.Name);
                std::wstring name;
                for (size_t i = 0; i < 128; ++i)
                {
                    const auto c = image.Read<unsigned char>(start + i);
                    if (!c)
                        break;
                    if (!(isalnum(c) || c == '-' || c == '_' || c == '.'))
                        throw std::runtime_error(message("#NextPlugins_ErrorUnsafeImport"));
                    name += static_cast<wchar_t>(c);
                }
                if (name.empty() || name.size() >= 128)
                    throw std::runtime_error(message("#NextPlugins_ErrorInvalidImport"));
                result.push_back(lower(name));
            }
            throw std::runtime_error(message("#NextPlugins_ErrorImportLimit"));
        }
    } // namespace
    Package::~Package()
    {
        for (auto it = modules.rbegin(); it != modules.rend(); ++it)
            FreeLibrary(*it);
        for (auto handle : directories)
            CloseHandle(handle);
    }
    std::unique_ptr<Package> read_package(const fs::path& path)
    {
        auto package = std::make_unique<Package>();
        auto lock_directory = [&](const fs::path& directory) {
            if (package->directories.size() >= 128)
                throw std::runtime_error(message("#NextPlugins_ErrorPackageDirectoryLimit"));
            auto handle = CreateFileW(
                directory.c_str(),
                FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS,
                nullptr
            );
            if (handle == INVALID_HANDLE_VALUE)
                throw std::runtime_error(message("#NextPlugins_ErrorLockPackage"));
            package->directories.push_back(handle);
        };
        lock_directory(path.parent_path());
        if (GetFileAttributesW(path.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT)
            throw std::runtime_error(message("#NextPlugins_ErrorPackageLinks"));
        if (!fs::is_directory(path))
        {
            package->files.push_back(std::make_unique<File>(path));
            auto bytes = package->files.back()->Read();
            package->hash = sha256(bytes);
            package->manifest = manifest(pe_manifest(bytes));
            package->binaries.push_back(0);
            return package;
        }
        std::vector<fs::path> paths;
        lock_directory(path);
        for (const auto& file : fs::recursive_directory_iterator(path))
        {
            if (GetFileAttributesW(file.path().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT)
                throw std::runtime_error(message("#NextPlugins_ErrorPackageLinks"));
            if (file.is_regular_file())
                paths.push_back(file.path());
            else if (file.is_directory())
                lock_directory(file.path());
            if (paths.size() > 128)
                throw std::runtime_error(message("#NextPlugins_ErrorPackageFileLimit"));
        }
        std::sort(paths.begin(), paths.end());
        std::string digest;
        size_t total{};
        bool found{};
        for (const auto& file : paths)
        {
            const auto index = package->files.size();
            package->files.push_back(std::make_unique<File>(file));
            const auto bytes = package->files.back()->Read(true);
            total += bytes.size();
            if (total > 256 * 1024 * 1024)
                throw std::runtime_error(message("#NextPlugins_ErrorPackageSize"));
            const auto relative = file.lexically_relative(path).generic_u8string();
            digest += std::to_string(relative.size()) + ":" + std::string(relative.begin(), relative.end()) + ":" + sha256(bytes) + "\n";
            if (lower(file.extension().wstring()) == L".dll")
            {
                if (file.parent_path() != path)
                    throw std::runtime_error(message("#NextPlugins_ErrorNestedDll"));
                imports(bytes);
                package->binaries.push_back(index);
                if (lower(file.filename().wstring()) == L"plugin.dll")
                {
                    package->entry = index;
                    package->manifest = manifest(pe_manifest(bytes));
                    found = true;
                }
            }
        }
        if (!found)
            throw std::runtime_error(message("#NextPlugins_ErrorMissingPluginDll"));
        package->hash = sha256(std::vector<unsigned char>(digest.begin(), digest.end()));
        return package;
    }
    HMODULE Package::Load()
    {
        std::map<std::wstring, size_t> names;
        std::map<size_t, std::vector<std::wstring>> dependencies;
        for (auto i : binaries)
        {
            const auto name = lower(fs::path(files[i]->ResolvedPath()).filename().wstring());
            // Entry points may share the conventional plugin.dll filename. Imports
            // must never bind to an entry point belonging to another package.
            if (i != entry && GetModuleHandleW(name.c_str()))
                throw std::runtime_error(message("#NextPlugins_ErrorCompanionCollision", {filename_utf8(name)}));
            if (!names.emplace(name, i).second)
                throw std::runtime_error(message("#NextPlugins_ErrorDuplicateDll", {filename_utf8(name)}));
            dependencies[i] = imports(files[i]->Read());
        }
        std::vector<size_t> order;
        std::set<size_t> visiting, visited;
        std::function<void(size_t)> visit = [&](size_t index) {
            if (visited.count(index))
                return;
            if (!visiting.insert(index).second)
                throw std::runtime_error(message("#NextPlugins_ErrorCircularImports"));
            for (const auto& name : dependencies[index])
            {
                if (auto it = names.find(name); it != names.end())
                {
                    if (it->second == entry)
                        throw std::runtime_error(message("#NextPlugins_ErrorEntryImport", {filename_utf8(name)}));
                    visit(it->second);
                }
                else if (!GetModuleHandleW(name.c_str()))
                {
                    // Preload external imports from System32 only. No current
                    // directory or package-directory search is ever enabled.
                    auto module = LoadLibraryExW(name.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
                    if (!module)
                        throw std::runtime_error(message("#NextPlugins_ErrorSystemImport", {filename_utf8(name)}));
                    modules.push_back(module);
                }
            }
            visiting.erase(index);
            visited.insert(index);
            order.push_back(index);
        };
        // Validate the full graph before executing any package code.
        for (auto i : binaries)
            visit(i);
        HMODULE result{};
        for (auto i : order)
        {
            auto module = LoadLibraryExW(files[i]->ResolvedPath().c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!module)
                throw std::runtime_error(message("#NextPlugins_ErrorLoadPackageDll", {filename_utf8(files[i]->ResolvedPath())}));
            modules.push_back(module);
            if (i == entry)
                result = module;
        }
        return result;
    }
} // namespace plugins
