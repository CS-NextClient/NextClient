#include "plugin_package.h"
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
        template <class T>
        T read(const std::vector<unsigned char>& bytes, size_t offset)
        {
            if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
                throw std::runtime_error("Truncated PE");
            T result;
            std::memcpy(&result, bytes.data() + offset, sizeof(T));
            return result;
        }
        std::vector<std::wstring> imports(const std::vector<unsigned char>& bytes)
        {
            const auto dos = read<IMAGE_DOS_HEADER>(bytes, 0);
            if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0)
                throw std::runtime_error("Invalid PE");
            const auto nt = read<IMAGE_NT_HEADERS32>(bytes, dos.e_lfanew);
            if (nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
                nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC || !(nt.FileHeader.Characteristics & IMAGE_FILE_DLL))
                throw std::runtime_error("Only x86 DLLs are supported");
            if (nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].Size)
                throw std::runtime_error("Package binaries must not use delay imports");
            auto offset = [&](uint32_t rva) -> size_t {
                for (size_t n = 0; n < nt.FileHeader.NumberOfSections; ++n)
                {
                    const auto section = read<IMAGE_SECTION_HEADER>(
                        bytes,
                        static_cast<size_t>(dos.e_lfanew) + 24 + nt.FileHeader.SizeOfOptionalHeader + n * sizeof(IMAGE_SECTION_HEADER)
                    );
                    if (rva >= section.VirtualAddress && rva - section.VirtualAddress < section.SizeOfRawData)
                        return static_cast<size_t>(section.PointerToRawData) + (rva - section.VirtualAddress);
                }
                throw std::runtime_error("Invalid PE address");
            };
            std::vector<std::wstring> result;
            const auto table = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            if (!table.VirtualAddress)
                return result;
            for (size_t n = 0; n < 512; ++n)
            {
                const auto row = read<IMAGE_IMPORT_DESCRIPTOR>(bytes, offset(table.VirtualAddress) + n * sizeof(IMAGE_IMPORT_DESCRIPTOR));
                if (!row.Name)
                    return result;
                auto start = offset(row.Name);
                std::wstring name;
                for (size_t i = 0; i < 128; ++i)
                {
                    const auto c = read<unsigned char>(bytes, start + i);
                    if (!c)
                        break;
                    if (!(isalnum(c) || c == '-' || c == '_' || c == '.'))
                        throw std::runtime_error("Unsafe DLL import name");
                    name += static_cast<wchar_t>(c);
                }
                if (name.empty() || name.size() >= 128)
                    throw std::runtime_error("Invalid DLL import name");
                result.push_back(lower(name));
            }
            throw std::runtime_error("Import limit exceeded");
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
                throw std::runtime_error("Package directory limit exceeded");
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
                throw std::runtime_error("Cannot lock package directory");
            package->directories.push_back(handle);
        };
        lock_directory(path.parent_path());
        if (GetFileAttributesW(path.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT)
            throw std::runtime_error("Package links are not supported");
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
                throw std::runtime_error("Package links are not supported");
            if (file.is_regular_file())
                paths.push_back(file.path());
            else if (file.is_directory())
                lock_directory(file.path());
            if (paths.size() > 128)
                throw std::runtime_error("Package file limit exceeded");
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
                throw std::runtime_error("Package size limit exceeded");
            const auto relative = file.lexically_relative(path).generic_u8string();
            digest += std::to_string(relative.size()) + ":" + std::string(relative.begin(), relative.end()) + ":" + sha256(bytes) + "\n";
            if (lower(file.extension().wstring()) == L".dll")
            {
                if (file.parent_path() != path)
                    throw std::runtime_error("DLLs must be in the package root");
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
            throw std::runtime_error("Package is missing plugin.dll");
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
                throw std::runtime_error("Companion DLL name already loaded");
            if (!names.emplace(name, i).second)
                throw std::runtime_error("Duplicate DLL name");
            dependencies[i] = imports(files[i]->Read());
        }
        std::vector<size_t> order;
        std::set<size_t> visiting, visited;
        std::function<void(size_t)> visit = [&](size_t index) {
            if (visited.count(index))
                return;
            if (!visiting.insert(index).second)
                throw std::runtime_error("Circular package imports");
            for (const auto& name : dependencies[index])
            {
                if (auto it = names.find(name); it != names.end())
                {
                    if (it->second == entry)
                        throw std::runtime_error("Companions cannot import the plugin entry DLL");
                    visit(it->second);
                }
                else if (!GetModuleHandleW(name.c_str()))
                {
                    // Preload external imports from System32 only. No current
                    // directory or package-directory search is ever enabled.
                    auto module = LoadLibraryExW(name.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
                    if (!module)
                        throw std::runtime_error("External dependency unavailable in System32");
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
                throw std::runtime_error("Cannot load approved DLL");
            modules.push_back(module);
            if (i == entry)
                result = module;
        }
        return result;
    }
} // namespace plugins
