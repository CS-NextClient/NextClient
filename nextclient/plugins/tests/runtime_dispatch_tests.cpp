#include <gtest/gtest.h>
#include <nextclient/runtime.h>
#include "catalog.h"
#include <windows.h>
#include <algorithm>

namespace
{
    using namespace plugins;
    namespace fs = std::filesystem;
    std::vector<uint32_t> rendered;
    void Rect(int32_t, int32_t, int32_t, int32_t, uint32_t color)
    {
        rendered.push_back(color);
    }
    void Text(int32_t, int32_t, const char*, uint32_t color)
    {
        rendered.push_back(color);
    }
    int32_t Watch(const char*)
    {
        return 1;
    }
    enum Category
    {
        Event,
        Frame,
        Draw,
        Command,
        Filter,
        Count
    };
} // namespace

class DispatchRuntime : public ::testing::Test
{
protected:
    fs::path dir;
    NcClientServices services{};
    template <class T>
    T symbol(int owner, const char* name)
    {
        return reinterpret_cast<T>(GetProcAddress(GetModuleHandleW(owner ? L"dispatch-b.dll" : L"dispatch-a.dll"), name));
    }
    void SetUp() override
    {
        dir = fs::temp_directory_path() /
              (L"nextclient-dispatch-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        fs::create_directories(dir / L"plugins");
        fs::copy_file(PLUGIN_DISPATCH_A_PATH, dir / L"plugins" / L"dispatch-a.dll");
        fs::copy_file(PLUGIN_DISPATCH_B_PATH, dir / L"plugins" / L"dispatch-b.dll");
        services.draw_rect = Rect;
        services.draw_text = Text;
        services.watch_message = Watch;
        nc_runtime_bind_client(&services);
        nc_runtime_start(dir.c_str(), 0);
        auto rows = parse(nc_runtime_catalog()).at("plugins");
        ASSERT_EQ(rows.get_array().size(), 2);
        for (size_t i = 0; i < rows.get_array().size(); ++i)
        {
            rows.get_array()[i]["enabled"] = rows.get_array()[i]["consent"] = true;
            rows.get_array()[i]["order"] = i;
        }
        ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
        nc_runtime_stop();
        nc_runtime_start(dir.c_str(), 0);
        for (int owner = 0; owner < 2; ++owner)
            ASSERT_NE(symbol<const NcHost*(NC_CALL*)()>(owner, "nc_dispatch_host"), nullptr);
        rendered.clear();
    }
    void TearDown() override
    {
        nc_runtime_stop();
        nc_runtime_bind_client(nullptr);
        fs::remove_all(dir);
    }
    void delay(int owner, Category category, int milliseconds)
    {
        symbol<void(NC_CALL*)(int, int)>(owner, "nc_dispatch_delay")(category, milliseconds);
    }
    int count(int owner, Category category)
    {
        return symbol<int(NC_CALL*)(int)>(owner, "nc_dispatch_count")(category);
    }
    void frame()
    {
        const NcSession context{sizeof(NcSession)};
        nc_runtime_frame(&context);
    }
    void draw()
    {
        rendered.clear();
        const NcDrawContext context{sizeof(NcDrawContext)};
        nc_runtime_draw(&context);
    }
    uint32_t command()
    {
        NcCommand value{sizeof(NcCommand)};
        const NcPlayer player{sizeof(NcPlayer)};
        nc_runtime_command(&value, &player);
        return value.buttons;
    }
    int filter()
    {
        const uint8_t input[6]{};
        uint8_t output[6]{};
        uint32_t size = sizeof(output);
        EXPECT_EQ(nc_runtime_message("ScreenShake", input, sizeof(input), 0, 0, output, &size), 1);
        return output[0];
    }
    Json stats()
    {
        const auto* host = symbol<const NcHost*(NC_CALL*)()>(0, "nc_dispatch_host")();
        const auto* api = host->query_interface(host->context, "nextclient.events", 1);
        const auto result = api->call(host->context, "stats", "{}");
        const auto size = api->read_result(host->context, result, nullptr, 0);
        std::string value(size, '\0');
        api->read_result(host->context, result, value.data(), size);
        api->release_result(host->context, result);
        value.pop_back();
        return parse(value);
    }
};

TEST_F(DispatchRuntime, DrawingDeadlineNeverChangesConfiguredLayering)
{
    delay(0, Draw, 4);
    delay(1, Draw, 4);
    frame();
    draw();
    ASSERT_EQ(rendered.size(), 2);
    // Force the next scheduling pass to begin with the second configured owner.
    if (rendered.front() == 2)
    {
        frame();
        draw();
        ASSERT_EQ(rendered, (std::vector<uint32_t>{1, 1}));
    }
    delay(0, Draw, 0);
    delay(1, Draw, 0);
    frame();
    draw();
    EXPECT_EQ(rendered, (std::vector<uint32_t>{1, 1, 2, 2}));
}

TEST_F(DispatchRuntime, SlowQueuedEventsCannotStarveOtherCategoriesOrOwners)
{
    delay(0, Event, 8);
    delay(1, Event, 8);
    for (int pump = 0; pump < 6; ++pump)
    {
        nc_runtime_event("player.health", "{}");
        frame();
        draw();
        command();
        filter();
    }
    for (int owner = 0; owner < 2; ++owner)
        for (int category = 0; category < Count; ++category)
            EXPECT_GE(count(owner, static_cast<Category>(category)), 3) << owner << ":" << category;
}

TEST_F(DispatchRuntime, CommandAndFilterProgressKeepsConfiguredChainOrder)
{
    delay(0, Command, 8);
    delay(0, Filter, 8);
    bool command_second{}, filter_second{};
    for (int pump = 0; pump < 4; ++pump)
    {
        frame();
        const auto buttons = command();
        EXPECT_TRUE(buttons == 1 || buttons == 12);
        command_second |= buttons == 12;
        const auto filtered = filter();
        EXPECT_TRUE(filtered == 1 || filtered == 2 || filtered == 12);
        filter_second |= filtered == 2 || filtered == 12;
    }
    EXPECT_TRUE(command_second);
    EXPECT_TRUE(filter_second);
}

TEST_F(DispatchRuntime, DeferredCountIncludesTwoMillisecondFrameAndDrawDeadlines)
{
    delay(0, Frame, 4);
    delay(1, Frame, 4);
    delay(0, Draw, 4);
    delay(1, Draw, 4);
    const auto before = stats().at("deferred_callbacks").as<uint64_t>();
    frame();
    EXPECT_EQ(stats().at("deferred_callbacks").as<uint64_t>(), before + 1);
    draw();
    EXPECT_EQ(stats().at("deferred_callbacks").as<uint64_t>(), before + 2);
}

TEST_F(DispatchRuntime, DeferredCountTracksEachEligibleQueuedEvent)
{
    delay(0, Event, 8);
    delay(1, Event, 8);
    for (int i = 0; i < 3; ++i)
        nc_runtime_event("player.health", "{}");
    const auto before = stats().at("deferred_callbacks").as<uint64_t>();
    frame();
    EXPECT_EQ(count(0, Event) + count(1, Event), 1);
    EXPECT_EQ(stats().at("deferred_callbacks").as<uint64_t>(), before + 6); // Five events and one frame callback.
}
