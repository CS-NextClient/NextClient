#include "pe_image.h"
#include <gtest/gtest.h>
#include <fstream>
#include <set>

namespace
{
    std::vector<unsigned char> ReadImage(const char* path)
    {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), {}};
    }
    std::set<std::string> ImportedFunctions(const std::vector<unsigned char>& bytes)
    {
        const plugins::PeImage image(bytes);
        std::set<std::string> names;
        const auto imports = image.optional.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!imports.VirtualAddress)
            return names;
        for (size_t i = 0;; ++i)
        {
            const auto row =
                image.Read<IMAGE_IMPORT_DESCRIPTOR>(image.Offset(imports.VirtualAddress) + i * sizeof(IMAGE_IMPORT_DESCRIPTOR));
            if (!row.Name)
                break;
            const auto first = image.Offset(row.OriginalFirstThunk ? row.OriginalFirstThunk : row.FirstThunk);
            for (size_t j = 0;; ++j)
            {
                const auto thunk = image.Read<DWORD>(first + j * sizeof(DWORD));
                if (!thunk)
                    break;
                if (IMAGE_SNAP_BY_ORDINAL32(thunk))
                    continue;
                const auto start = image.Offset(thunk) + sizeof(WORD);
                std::string name;
                for (size_t k = start; const auto c = image.Read<char>(k); ++k)
                    name += c;
                names.insert(name);
            }
        }
        return names;
    }
} // namespace
TEST(PluginBinaries, ShippedDllsDoNotDirectlyImportWindows8Apis)
{
    for (const auto* path : {PLUGIN_RUNTIME_PATH, PLUGIN_LIFE_STATS_PATH})
    {
        SCOPED_TRACE(path);
        const auto names = ImportedFunctions(ReadImage(path));
        ASSERT_FALSE(names.empty());
        for (const auto* name : {"CreateFile2", "CopyFile2", "GetSystemTimePreciseAsFileTime"})
            EXPECT_FALSE(names.contains(name)) << name;
    }
}
TEST(PluginMetadata, SharedPeParserRejectsOutOfFileSectionsAndUnmappedAddresses)
{
    auto bytes = ReadImage(PLUGIN_LIFE_STATS_PATH);
    const plugins::PeImage valid(bytes);
    EXPECT_THROW(valid.Offset(0xffffffff), std::exception);
    const auto dos = valid.Read<IMAGE_DOS_HEADER>(0);
    const auto start = dos.e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + valid.header.SizeOfOptionalHeader;
    auto section = valid.Section(0);
    section.PointerToRawData = 0xfffffff0;
    std::memcpy(bytes.data() + start, &section, sizeof(section));
    EXPECT_THROW((void)plugins::PeImage(bytes), std::exception);
}
