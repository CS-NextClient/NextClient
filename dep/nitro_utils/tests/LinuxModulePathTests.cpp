#include <memory>

#include <gtest/gtest.h>

#include <nitro_utils/MemoryTools.h>
#include <nitro_utils/platform.h>

namespace
{
    class LinuxModulePathTest : public testing::TestWithParam<const char*>
    {};
} // namespace

TEST_P(LinuxModulePathTest, ResolvesSymbolsFromDirectoryWithSpaces)
{
    std::unique_ptr<void, decltype(&nitro_utils::UnloadSysModule)> library(
        nitro_utils::LoadSysModule(GetParam()), nitro_utils::UnloadSysModule
    );
    ASSERT_NE(library, nullptr);

    MemoryModule module(library.get());
    MemScanner scanner(module);
    ASSERT_EQ(scanner.Module(), library.get());

    using read_value_func_t = int (*)();
    read_value_func_t read_value =
        reinterpret_cast<read_value_func_t>(nitro_utils::GetProcAddress(scanner.Module(), "TestModule_ReadValue"));
    ASSERT_NE(read_value, nullptr);
    EXPECT_EQ(read_value(), 42);
}

INSTANTIATE_TEST_SUITE_P(LinuxPaths, LinuxModulePathTest, testing::Values(NITRO_UTILS_TEST_MODULE_PATH, NITRO_UTILS_TEST_LONG_MODULE_PATH));
