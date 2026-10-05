#include "plugin_file.h"
#include <gtest/gtest.h>
#include <fstream>

using namespace plugins;
namespace fs = std::filesystem;
TEST(PluginConfigFile, OversizedWriteLeavesTheReadablePredecessorUntouched)
{
    const auto folder = fs::temp_directory_path() / (L"nc-config-" + std::to_wstring(GetCurrentProcessId()));
    const auto path = folder / L"settings.json";
    Json value{{"payload", std::string(1048556, 'a')}};
    ASSERT_EQ(tao::json::to_string(value).size(), 1048570u);
    write_json(path, value);
    value["payload"].get_string().append(30, 'b');
    ASSERT_EQ(tao::json::to_string(value).size(), 1048600u);
    EXPECT_THROW(write_json(path, value), std::exception);
    EXPECT_EQ(fs::file_size(path), 1048570u);
    EXPECT_NO_THROW(parse(read_text(path)));
    fs::remove_all(folder);
}
TEST(PluginConfigFile, WriterRejectsUnreadableNestingBeforeReplacement)
{
    const auto folder = fs::temp_directory_path() / (L"nc-depth-" + std::to_wstring(GetCurrentProcessId()));
    const auto path = folder / L"settings.json";
    write_json(path, Json{{"enabled", 1}});
    Json deep = 1;
    for (int i = 0; i < 18; ++i)
        deep = Json{{"nested", std::move(deep)}};
    EXPECT_THROW(write_json(path, deep), std::exception);
    EXPECT_EQ(parse(read_text(path)).at("enabled"), 1);
    fs::remove_all(folder);
}
