#include <gtest/gtest.h>
#include <nextclient/plugin.h>
#include <tao/json.hpp>
#include <windows.h>
#include <cstddef>
#include <cstdlib>

TEST(SdkAbi, EveryRustFieldMatchesTheCompiledCLayout)
{
    const auto path = _wgetenv(L"NEXTCLIENT_RUST_EVENTS_DLL");
    if (!path)
        GTEST_SKIP() << "Set NEXTCLIENT_RUST_EVENTS_DLL to the abi-tests build of rust-events";
    const auto module = LoadLibraryW(path);
    ASSERT_NE(module, nullptr);
    struct ModuleGuard
    {
        HMODULE module;
        ~ModuleGuard()
        {
            FreeLibrary(module);
        }
    } guard{module};
    const auto layout = reinterpret_cast<const char*(NC_CALL*)()>(GetProcAddress(module, "nc_test_abi_layout"));
    ASSERT_NE(layout, nullptr);
    const auto rust = tao::json::from_string(layout());
#define NC_ABI_TYPE(c, r)                                     \
    EXPECT_EQ(rust.at(#c ".sizeof").as<size_t>(), sizeof(c)); \
    EXPECT_EQ(rust.at(#c ".alignof").as<size_t>(), alignof(c));
#define NC_ABI_FIELD(c, r, field) EXPECT_EQ(rust.at(#c "." #field).as<size_t>(), offsetof(c, field)) << #c "." #field;
#include "../../../sdk/tests/abi_layout.def"
#undef NC_ABI_FIELD
#undef NC_ABI_TYPE
}
