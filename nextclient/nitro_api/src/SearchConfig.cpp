#include "SearchConfig.h"

#include <utility>
#include <iostream>

#ifndef _WIN32
    #include <dlfcn.h>
    #include <cstring>
    #include <elf.h>
    #include <link.h>
    #include <fstream>
    #include <unordered_map>
    #include <vector>
#endif

namespace nitroapi
{
#ifndef _WIN32
    namespace
    {
        using SymbolTable = std::unordered_map<std::string, uint32_t>;

        // Reads the full .symtab from the file on disk - unlike dlsym, this also
        // sees static (file-local) symbols, which Valve's hw.so keeps unstripped.
        SymbolTable ReadElfSymbolTable(const char* path)
        {
            SymbolTable table;

            std::ifstream file(path, std::ios::binary);
            if (!file)
                return table;

            Elf32_Ehdr header{};
            file.read(reinterpret_cast<char*>(&header), sizeof(header));
            if (!file || memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 || header.e_ident[EI_CLASS] != ELFCLASS32)
                return table;

            std::vector<Elf32_Shdr> sections(header.e_shnum);
            file.seekg(header.e_shoff);
            file.read(reinterpret_cast<char*>(sections.data()), sections.size() * sizeof(Elf32_Shdr));
            if (!file)
                return table;

            for (const Elf32_Shdr& section : sections)
            {
                if (section.sh_type != SHT_SYMTAB || section.sh_link >= sections.size())
                    continue;

                std::vector<Elf32_Sym> symbols(section.sh_size / sizeof(Elf32_Sym));
                file.seekg(section.sh_offset);
                file.read(reinterpret_cast<char*>(symbols.data()), symbols.size() * sizeof(Elf32_Sym));

                const Elf32_Shdr& strtab_section = sections[section.sh_link];
                std::vector<char> strtab(strtab_section.sh_size + 1, '\0');
                file.seekg(strtab_section.sh_offset);
                file.read(strtab.data(), strtab_section.sh_size);

                if (!file)
                    return table;

                for (const Elf32_Sym& symbol : symbols)
                {
                    if (symbol.st_value == 0 || symbol.st_shndx == SHN_UNDEF || symbol.st_name >= strtab_section.sh_size)
                        continue;

                    table.try_emplace(&strtab[symbol.st_name], symbol.st_value);
                }
            }

            return table;
        }

        uint32_t FindSymbolAddress(nitro_utils::SysModule module, const std::string& name)
        {
            link_map* linkmap = nullptr;
            if (module == nullptr || dlinfo(module, RTLD_DI_LINKMAP, &linkmap) != 0 || linkmap == nullptr)
                return 0;

            static std::unordered_map<std::string, SymbolTable> tables;
            auto [it, inserted] = tables.try_emplace(linkmap->l_name);
            if (inserted)
                it->second = ReadElfSymbolTable(linkmap->l_name);

            auto symbol = it->second.find(name);
            if (symbol == it->second.end())
                return 0;

            return static_cast<uint32_t>(linkmap->l_addr) + symbol->second;
        }
    }
#endif

    SearchConfig::SearchConfig(std::string search_string, SearchType search_type) :
        search_string(std::move(search_string)),
        search_type(search_type),
        offset(0)
    { }

    SearchConfig::SearchConfig(uint32_t offset) :
        search_string(),
        search_type(SearchType::Offset),
        offset(offset)
    { }

    SearchConfig::SearchConfig(std::string search_string, uint32_t offset, SearchType search_type) :
        search_string(std::move(search_string)),
        offset(offset),
        search_type(search_type)
    { }

    uint32_t SearchConfig::FindAddress(MemScanner& mem_scanner) const
    {
        switch (search_type)
        {
            default:
            case SearchType::Pattern:
                return mem_scanner.FindPattern(search_string.c_str());

            case SearchType::PatternEx:
                return mem_scanner.FindPattern2(search_string);

            case SearchType::ExportFunc:
                return (uint32_t)nitro_utils::GetProcAddress(mem_scanner.Module(), search_string.c_str());

            case SearchType::Symbol:
#ifdef _WIN32
                return 0;
#else
                return FindSymbolAddress(mem_scanner.Module(), search_string);
#endif

            case SearchType::Offset:
                return mem_scanner.ModuleStart() + offset;

            case SearchType::PatternAndOffset:
                return mem_scanner.FindPattern(search_string.c_str()) + offset;

            case SearchType::PatternExAndOffset:
                return mem_scanner.FindPattern2(search_string) + offset;

            case SearchType::String:
                return mem_scanner.FindString(search_string.c_str());

            case SearchType::NotPresent:
                return 0;
        }
    }
}