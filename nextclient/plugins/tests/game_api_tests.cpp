#include <gtest/gtest.h>
#include <nextclient/runtime.h>
#include "catalog.h"
#include <windows.h>
#include <cstring>
#include <fstream>
#include <thread>

namespace
{
    using namespace plugins;
    namespace fs = std::filesystem;
    std::vector<std::string> created, sent, connections, local_chat;
    int disconnects, reads, writes;
    bool reject_creation;
    int32_t Create(const char* name, const char* value, int32_t archive)
    {
        created.push_back(std::string(name) + "=" + value + ":" + std::to_string(archive));
        return !reject_creation;
    }
    int32_t Chat(const char* text, int32_t team)
    {
        sent.push_back(std::to_string(team) + ":" + text);
        return 1;
    }
    int32_t Connect(const char* host, uint32_t port)
    {
        connections.push_back(std::string(host) + ":" + std::to_string(port));
        return 1;
    }
    int32_t Disconnect()
    {
        ++disconnects;
        return 1;
    }
    int32_t ChatPrint(const char* text)
    {
        local_chat.push_back(text);
        return 1;
    }
    uint32_t Data(const char* section, int32_t, char* out, uint32_t capacity)
    {
        ++reads;
        const auto text = tao::json::to_string(Json{{"section", section}, {"health", 73}, {"state", "active"}});
        const auto size = static_cast<uint32_t>(text.size() + 1);
        if (out && capacity >= size)
            std::memcpy(out, text.c_str(), size);
        return size;
    }
    int32_t Write(const char* name, const char* value)
    {
        ++writes;
        nc_runtime_cvar_changed(name, "250", value);
        // An engine callback during an SDK call must not re-enter the plugin.
        const NcSession frame{sizeof(NcSession)};
        nc_runtime_frame(&frame);
        return 1;
    }
    template <class T>
    T Symbol(const wchar_t* file, const char* name)
    {
        return reinterpret_cast<T>(GetProcAddress(GetModuleHandleW(file), name));
    }
} // namespace
class GameApiRuntime : public ::testing::Test
{
protected:
    fs::path dir;
    const NcHost* host{};
    NcClientServices services{};
    void SetUp() override
    {
        dir = fs::temp_directory_path() /
              (L"nextclient-game-api-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        fs::create_directories(dir / L"plugins");
        created.clear();
        sent.clear();
        connections.clear();
        local_chat.clear();
        disconnects = reads = writes = 0;
        reject_creation = false;
        services.game_data = Data;
        services.create_cvar = Create;
        services.send_chat = Chat;
        services.connect = Connect;
        services.disconnect = Disconnect;
        services.chat_print = ChatPrint;
        services.write_cvar = Write;
        nc_runtime_bind_client(&services);
    }
    void TearDown() override
    {
        nc_runtime_stop();
        nc_runtime_bind_client(nullptr);
        fs::remove_all(dir);
    }
    Json rows()
    {
        return parse(nc_runtime_catalog()).at("plugins");
    }
    void install(const char* path, bool other = false)
    {
        fs::copy_file(path, dir / L"plugins" / L"game-api.dll");
        if (other)
            fs::copy_file(PLUGIN_GAME_OTHER_PATH, dir / L"plugins" / L"game-other.dll");
        nc_runtime_start(dir.c_str(), 0);
        auto plugins = rows();
        for (auto& row : plugins.get_array())
            row["enabled"] = row["consent"] = true;
        ASSERT_STREQ(nc_runtime_save(tao::json::to_string(plugins).c_str()), "");
        restart();
    }
    void restart()
    {
        nc_runtime_stop();
        nc_runtime_start(dir.c_str(), 0);
        auto get = Symbol<const NcHost*(NC_CALL*)()>(L"game-api.dll", "nc_test_host");
        host = get ? get() : nullptr;
    }
    int count()
    {
        return Symbol<int(NC_CALL*)()>(L"game-api.dll", "nc_test_count")();
    }
    int creation()
    {
        return Symbol<int(NC_CALL*)()>(L"game-api.dll", "nc_test_creation")();
    }
    void mode(int v)
    {
        Symbol<void(NC_CALL*)(int)>(L"game-api.dll", "nc_test_mode")(v);
    }
    std::string name(int i)
    {
        return Symbol<const char*(NC_CALL*)(int)>(L"game-api.dll", "nc_test_name")(i);
    }
    Json payload(int i)
    {
        return parse(Symbol<const char*(NC_CALL*)(int)>(L"game-api.dll", "nc_test_json")(i));
    }
    void frame()
    {
        const NcSession value{sizeof(NcSession)};
        nc_runtime_frame(&value);
    }
    std::string get(const char* key)
    {
        const auto size = host->store_get(host->context, key, nullptr, 0);
        if (!size)
            return {};
        std::string value(size, '\0');
        EXPECT_EQ(host->store_get(host->context, key, value.data(), size), size);
        value.resize(size - 1);
        return value;
    }
    Json extension(const char* interface_name, const char* operation, const Json& args = tao::json::empty_object)
    {
        const auto* api = host->query_interface(host->context, interface_name, 1);
        if (!api)
        {
            ADD_FAILURE() << interface_name;
            return Json{{"ok", false}};
        }
        const auto handle = api->call(host->context, operation, tao::json::to_string(args).c_str());
        const auto size = api->read_result(host->context, handle, nullptr, 0);
        if (!size)
        {
            ADD_FAILURE() << operation;
            return Json{{"ok", false}};
        }
        std::string value(size, '\0');
        EXPECT_EQ(api->read_result(host->context, handle, value.data(), size), size);
        api->release_result(host->context, handle);
        value.pop_back();
        return parse(value);
    }
};
TEST_F(GameApiRuntime, ExtensionVersionsResultsAndWorkerTokens)
{
    install(PLUGIN_GAME_PATH);
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(host->query_interface(host->context, "nextclient.tasks", 2), nullptr);
    EXPECT_EQ(host->query_interface(host->context, "unknown", 1), nullptr);
    const auto* api = host->query_interface(host->context, "nextclient.tasks", 1);
    const auto token = extension("nextclient.tasks", "token").at("token").as<uint64_t>();
    int accepted{};
    std::thread worker([&] { accepted = api->post(token, R"({"worker":true})"); });
    worker.join();
    EXPECT_EQ(accepted, 1);
    EXPECT_EQ(count(), 0);
    frame();
    ASSERT_EQ(count(), 1);
    EXPECT_EQ(name(0), "sdk.task");
    EXPECT_EQ(payload(0).at("worker"), true);
    restart();
    EXPECT_EQ(api->post(token, "{}"), 0);
}
TEST_F(GameApiRuntime, AsyncStoreSerializesMutationsAndDrainsOnShutdown)
{
    install(PLUGIN_GAME_PATH);
    ASSERT_NE(host, nullptr);
    ASSERT_EQ(host->store_set(host->context, "old", "1"), 1);
    auto result = extension("nextclient.storage", "commit", Json{{"set", Json{{"new", 2}}}, {"delete", Json::array({"old"})}});
    ASSERT_EQ(result.at("ok"), true);
    EXPECT_EQ(host->store_set(host->context, "old", "3"), 0);
    EXPECT_EQ(extension("nextclient.storage", "commit").at("ok"), false);
    EXPECT_EQ(get("old"), "1");
    restart();
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(get("new"), "2");
    EXPECT_EQ(get("old"), "");
}
TEST_F(GameApiRuntime, EventOverflowIsReportedAndFramesHaveDeliveryBudgets)
{
    install(PLUGIN_GAME_PATH);
    ASSERT_NE(host, nullptr);
    host->subscribe_event(host->context, "player.health", 1);
    for (int i = 0; i < 2050; ++i)
        nc_runtime_event("player.health", "{}");
    frame();
    ASSERT_GT(count(), 0);
    EXPECT_LE(count(), 129);
    EXPECT_EQ(name(0), "sdk.overflow");
    EXPECT_EQ(payload(0).at("dropped"), 2);
    const auto stats = extension("nextclient.events", "stats");
    EXPECT_EQ(stats.at("dropped"), 2);
    EXPECT_GT(stats.at("queued").as<int>(), 0);
}
TEST_F(GameApiRuntime, RawMessagesRequirePermissionAndKeepOriginalBytes)
{
    install(PLUGIN_GAME_PATH);
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(extension("nextclient.messages", "subscribe", Json{{"name", "Custom"}}).at("ok"), false);
    EXPECT_EQ(extension("nextclient.ui", "create").at("ok"), false);
    EXPECT_EQ(extension("nextclient.services", "lookup", Json{{"name", "test.game/test"}, {"version", 1}}).at("ok"), false);
}
TEST_F(GameApiRuntime, MenuPumpDeliversTasksWithoutClientServices)
{
    install(PLUGIN_GAME_PATH);
    ASSERT_NE(host, nullptr);
    nc_runtime_bind_client(nullptr);
    const auto* api = host->query_interface(host->context, "nextclient.tasks", 1);
    const auto token = extension("nextclient.tasks", "token").at("token").as<uint64_t>();
    EXPECT_EQ(api->post(token, "{}"), 1);
    nc_runtime_pump();
    ASSERT_EQ(count(), 1);
    EXPECT_EQ(name(0), "sdk.task");
}
TEST_F(GameApiRuntime, AsyncStoreFailureKeepsCommittedDataAndReportsFailure)
{
    install(PLUGIN_GAME_PATH);
    ASSERT_NE(host, nullptr);
    ASSERT_EQ(host->store_set(host->context, "value", "1"), 1);
    const auto file = dir / L"plugins/data/plugin-test.game.json";
    const auto lock = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(lock, INVALID_HANDLE_VALUE);
    ASSERT_EQ(extension("nextclient.storage", "commit", Json{{"set", Json{{"value", 2}}}}).at("ok"), true);
    for (int n = 0; n < 200 && count() == 0; ++n)
    {
        Sleep(5);
        frame();
    }
    CloseHandle(lock);
    ASSERT_EQ(count(), 1);
    EXPECT_EQ(name(0), "sdk.storage");
    EXPECT_EQ(payload(0).at("ok"), false);
    EXPECT_EQ(get("value"), "1");
}
TEST_F(GameApiRuntime, FiltersValidateRewritesAndPreserveObservation)
{
    install(PLUGIN_GAME_ALL_PATH);
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(extension("nextclient.messages", "subscribe", Json{{"name", "ScreenShake"}}).at("ok"), true);
    const auto* api = host->query_interface(host->context, "nextclient.messages", 1);
    auto filter = [](void*, const char*, const uint8_t*, uint32_t, uint8_t* out, uint32_t* size) -> int32_t {
        std::memset(out, 0, 6);
        *size = 6;
        return 1;
    };
    ASSERT_EQ(api->set_filter(host->context, "ScreenShake", filter, nullptr), 1);
    EXPECT_EQ(api->set_filter(host->context, "Health", filter, nullptr), 0);
    uint8_t bytes[6]{1, 2, 3, 4, 5, 6}, replacement[4096]{};
    uint32_t size = sizeof(replacement);
    EXPECT_EQ(nc_runtime_message("ScreenShake", bytes, 6, 3.0, 7, replacement, &size), 1);
    EXPECT_EQ(size, 6u);
    EXPECT_EQ(replacement[0], 0);
    frame();
    ASSERT_EQ(count(), 1);
    EXPECT_EQ(name(0), "sdk.message");
    EXPECT_EQ(payload(0).at("bytes").at(0), 1);
    auto invalid = [](void*, const char*, const uint8_t*, uint32_t, uint8_t*, uint32_t* length) -> int32_t {
        *length = 4097;
        return 1;
    };
    api->set_filter(host->context, "ScreenShake", invalid, nullptr);
    size = sizeof(replacement);
    EXPECT_EQ(nc_runtime_message("ScreenShake", bytes, 6, 3.0, 7, replacement, &size), 0);
    EXPECT_FALSE(rows().at(0).at("running").get_boolean());
}
TEST_F(GameApiRuntime, ServiceRequestsAreVersionedAndReplyHandlesAreSingleUse)
{
    install(PLUGIN_GAME_ALL_PATH, true);
    ASSERT_NE(host, nullptr);
    const Json service{{"name", "test.game/test"}, {"version", 1}};
    EXPECT_EQ(extension("nextclient.services", "lookup", service).at("ok"), true);
    EXPECT_EQ(extension("nextclient.services", "lookup", Json{{"name", "test.game/test"}, {"version", 2}}).at("ok"), false);
    EXPECT_EQ(extension("nextclient.services", "lookup", Json{{"name", "test.other/test"}, {"version", 1}}).at("ok"), false);
    auto request = service;
    request["method"] = "echo";
    request["data"] = 42;
    auto result = extension("nextclient.services", "request", request);
    ASSERT_EQ(result.at("ok"), true);
    frame();
    ASSERT_EQ(count(), 1);
    EXPECT_EQ(name(0), "sdk.service");
    Json reply{{"request", result.at("request")}, {"data", 42}};
    EXPECT_EQ(extension("nextclient.services", "reply", reply).at("ok"), true);
    EXPECT_EQ(extension("nextclient.services", "reply", reply).at("ok"), false);
    frame();
    ASSERT_EQ(count(), 2);
    EXPECT_EQ(name(1), "sdk.reply");
    EXPECT_EQ(payload(1).at("result").at("data"), 42);
}
TEST_F(GameApiRuntime, OwnedWindowsValidatePermissionsAndWidgetInput)
{
    install(PLUGIN_GAME_ALL_PATH);
    ASSERT_NE(host, nullptr);
    Json window{
        {"title", Json{{"en", "Test"}, {"ru", "Test"}}},
        {"surface", "all"},
        {"interactive", true},
        {"visible", true},
        {"width", 400},
        {"height", 300},
        {"items",
         Json::array({Json{{"id", "check"}, {"kind", "checkbox"}, {"text", Json{{"en", "Check"}, {"ru", "Check"}}}, {"value", false}}})}
    };
    const auto result = extension("nextclient.ui", "create", window);
    ASSERT_EQ(result.at("ok"), true);
    const auto handle = std::to_string(result.at("handle").as<uint64_t>());
    ASSERT_EQ(parse(nc_runtime_windows()).get_array().size(), 1u);
    nc_runtime_window_action(handle.c_str(), "check", "12");
    frame();
    EXPECT_EQ(count(), 0);
    nc_runtime_window_action(handle.c_str(), "check", "true");
    frame();
    ASSERT_EQ(count(), 1);
    EXPECT_EQ(name(0), "sdk.ui");
    EXPECT_EQ(payload(0).at("value"), true);
    EXPECT_EQ(extension("nextclient.ui", "destroy", Json{{"handle", result.at("handle")}}).at("ok"), true);
    EXPECT_TRUE(parse(nc_runtime_windows()).get_array().empty());
}
TEST_F(GameApiRuntime, ServicesCrossDeclaredDependenciesAndRetireWithProvider)
{
    install(PLUGIN_GAME_ALL_PATH);
    nc_runtime_stop();
    fs::copy_file(PLUGIN_SERVICE_CLIENT_PATH, dir / L"plugins" / L"services-client.dll");
    nc_runtime_start(dir.c_str(), 0);
    auto plugins = rows();
    for (auto& row : plugins.get_array())
        row["enabled"] = row["consent"] = true;
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(plugins).c_str()), "");
    restart();
    ASSERT_NE(host, nullptr);
    const auto clientGetter = Symbol<const NcHost*(NC_CALL*)()>(L"services-client.dll", "nc_test_host");
    ASSERT_NE(clientGetter, nullptr);
    const auto* caller = clientGetter();
    const auto* api = caller->query_interface(caller->context, "nextclient.services", 1);
    auto id = api->call(caller->context, "request", R"({"name":"test.game/test","version":1,"method":"echo","data":42})");
    char buffer[512]{};
    api->read_result(caller->context, id, buffer, sizeof(buffer));
    api->release_result(caller->context, id);
    const auto request = parse(buffer);
    ASSERT_EQ(request.at("ok"), true);
    frame();
    ASSERT_EQ(count(), 1);
    EXPECT_EQ(payload(0).at("caller"), "test.client");
    EXPECT_EQ(extension("nextclient.services", "reply", Json{{"request", request.at("request")}, {"data", 42}}).at("ok"), true);
    frame();
    auto clientCount = Symbol<int(NC_CALL*)()>(L"services-client.dll", "nc_test_count");
    EXPECT_EQ(clientCount(), 1);
    host->subscribe_event(host->context, "player.health", 1);
    mode(2);
    nc_runtime_event("player.health", "{}");
    frame();
    for (const auto& row : rows().get_array())
        EXPECT_FALSE(row.at("running").get_boolean());
    EXPECT_EQ(api->call(caller->context, "lookup", R"({"name":"test.game/test","version":1})"), 0u);
}
TEST_F(GameApiRuntime, SafeReadsAndStoreNeedNoPermissionsButActionsAreDenied)
{
    install(PLUGIN_GAME_PATH);
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(host->permissions(host->context), 0u);
    for (const auto* section : {"player", "entity", "entities", "weapon", "weapons", "ammo", "scoreboard", "match", "connection"})
        EXPECT_GT(host->game_data(host->context, section, 0, nullptr, 0), 0u);
    EXPECT_EQ(reads, 9);
    EXPECT_EQ(host->game_data(host->context, "cvars", 0, nullptr, 0), 0u);
    EXPECT_EQ(reads, 9);
    EXPECT_EQ(host->subscribe_event(host->context, "player.health", 1), 1);
    EXPECT_EQ(host->subscribe_event(host->context, "connection.changed", 1), 1);
    EXPECT_EQ(host->subscribe_event(host->context, "chat.message", 1), 0);
    EXPECT_EQ(host->subscribe_event(host->context, "unknown", 1), 0);
    EXPECT_EQ(host->watch_cvar(host->context, "speed", 1), 0);
    EXPECT_EQ(creation(), 0);
    EXPECT_EQ(host->send_chat(host->context, "hello", 0), 0);
    EXPECT_EQ(host->connect(host->context, "localhost", 27015), 0);
    EXPECT_EQ(host->disconnect(host->context), 0);
    EXPECT_TRUE(created.empty());
    EXPECT_TRUE(sent.empty());
    EXPECT_TRUE(connections.empty());
    EXPECT_EQ(disconnects, 0);
    EXPECT_EQ(host->store_set(host->context, "history", R"({"wins":2,"labels":["a","b"]})"), 1);
    EXPECT_EQ(parse(get("history")).at("wins"), 2);
    nc_runtime_event("chat.message", R"({"text":"private"})");
    nc_runtime_event("player.health", R"({"health":73})");
    EXPECT_EQ(count(), 0);
    frame();
    ASSERT_EQ(count(), 1);
    EXPECT_EQ(name(0), "player.health");
}
TEST_F(GameApiRuntime, SeparateReadAndConnectPermissionsDoNotGrantOtherActions)
{
    install(PLUGIN_GAME_READ_PATH);
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(host->watch_cvar(host->context, "speed", 1), 1);
    EXPECT_EQ(host->subscribe_event(host->context, "chat.message", 1), 1);
    EXPECT_EQ(creation(), 0);
    EXPECT_EQ(host->send_chat(host->context, "hello", 0), 0);
    EXPECT_EQ(host->connect(host->context, "localhost", 27015), 0);
    EXPECT_EQ(host->disconnect(host->context), 0);
    EXPECT_EQ(host->write_cvar(host->context, "speed", "125"), 0);
    nc_runtime_event("chat.message", R"({"text":"read permitted"})");
    frame();
    ASSERT_EQ(count(), 1);
    EXPECT_EQ(payload(0).at("text"), "read permitted");
}
TEST_F(GameApiRuntime, ConnectPermissionDoesNotGrantDisconnectChatOrCvarAccess)
{
    install(PLUGIN_GAME_CONNECT_PATH);
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(host->connect(host->context, "example.org", 27016), 1);
    EXPECT_EQ(connections, (std::vector<std::string>{"example.org:27016"}));
    EXPECT_EQ(host->disconnect(host->context), 0);
    EXPECT_EQ(host->send_chat(host->context, "hello", 0), 0);
    EXPECT_EQ(host->subscribe_event(host->context, "chat.message", 1), 0);
    EXPECT_EQ(host->watch_cvar(host->context, "speed", 1), 0);
    EXPECT_EQ(creation(), 0);
}
TEST_F(GameApiRuntime, CvarsRegisterAfterLoadAndDeferredClientBinding)
{
    nc_runtime_bind_client(nullptr);
    install(PLUGIN_GAME_ALL_PATH);
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(creation(), 1);
    EXPECT_TRUE(created.empty());
    nc_runtime_bind_client(&services);
    EXPECT_EQ(created, (std::vector<std::string>{"nc.test.game.counter=0:1"}));
    EXPECT_EQ(host->create_cvar(host->context, "late", "0", 0), 0);
    EXPECT_EQ(host->create_cvar(host->context, "other.counter", "0", 0), 0);
    reject_creation = true;
    nc_runtime_bind_client(&services);
    EXPECT_FALSE(rows().at(0).at("running").get_boolean());
    EXPECT_EQ(host->store_set(host->context, "retired", "true"), 0);
}
TEST_F(GameApiRuntime, ChatAndConnectionInputsCannotInjectEngineCommands)
{
    install(PLUGIN_GAME_ALL_PATH);
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(host->send_chat(host->context, "hello team", 1), 1);
    for (const auto* invalid : {"", "a;quit", "a\"", "a\\b", "a\nquit", "a\r", "a\t"})
        EXPECT_EQ(host->send_chat(host->context, invalid, 0), 0);
    EXPECT_EQ(host->send_chat(host->context, std::string(191, 'x').c_str(), 0), 0);
    EXPECT_EQ(host->send_chat(host->context, "x", 2), 0);
    EXPECT_EQ(sent, (std::vector<std::string>{"1:hello team"}));
    for (const auto* invalid : {"", "a;quit", "a\nquit", "a b", "a:27015", "a/b", "-host", "a..b"})
        EXPECT_EQ(host->connect(host->context, invalid, 27015), 0);
    EXPECT_EQ(host->connect(host->context, "localhost", 0), 0);
    EXPECT_EQ(host->connect(host->context, "localhost", 65536), 0);
    EXPECT_EQ(host->connect(host->context, "127.0.0.1", 27015), 1);
    EXPECT_EQ(host->disconnect(host->context), 1);
    EXPECT_EQ(connections.size(), 1u);
    EXPECT_EQ(disconnects, 1);
}
TEST_F(GameApiRuntime, SubscriptionsFilterChangesAndDoNotReplayOldEvents)
{
    install(PLUGIN_GAME_ALL_PATH);
    ASSERT_NE(host, nullptr);
    nc_runtime_event("player.health", R"({"health":1})");
    EXPECT_EQ(host->subscribe_event(host->context, "player.health", 1), 1);
    EXPECT_EQ(host->subscribe_event(host->context, "cvar.changed", 1), 0);
    EXPECT_EQ(host->watch_cvar(host->context, "SpEeD", 1), 1);
    nc_runtime_event("player.health", R"({"health":73})");
    nc_runtime_event("player.health", "invalid json");
    nc_runtime_cvar_changed("speed", "250", "250");
    nc_runtime_cvar_changed("other", "1", "2");
    nc_runtime_cvar_changed("SPEED", "250", "125");
    frame();
    ASSERT_EQ(count(), 2);
    EXPECT_EQ(payload(0).at("health"), 73);
    EXPECT_EQ(name(1), "cvar.changed");
    EXPECT_EQ(payload(1), (Json{{"name", "speed"}, {"old", "250"}, {"value", "125"}}));
    EXPECT_EQ(host->subscribe_event(host->context, "player.health", 0), 1);
    EXPECT_EQ(host->watch_cvar(host->context, "speed", 0), 1);
    nc_runtime_event("player.health", R"({"health":50})");
    nc_runtime_cvar_changed("speed", "125", "100");
    frame();
    EXPECT_EQ(count(), 2);
}
TEST_F(GameApiRuntime, CallbackGeneratedChangesWaitForNextFrameAndFailureRetiresPlugin)
{
    install(PLUGIN_GAME_ALL_PATH);
    ASSERT_NE(host, nullptr);
    host->subscribe_event(host->context, "player.health", 1);
    host->watch_cvar(host->context, "speed", 1);
    mode(1);
    nc_runtime_event("player.health", R"({"health":73})");
    frame();
    ASSERT_EQ(count(), 1);
    EXPECT_EQ(writes, 1);
    frame();
    ASSERT_EQ(count(), 2);
    EXPECT_EQ(name(1), "cvar.changed");
    mode(2);
    nc_runtime_event("player.health", "{}");
    nc_runtime_event("player.health", "{}");
    frame();
    EXPECT_EQ(count(), 3);
    EXPECT_FALSE(rows().at(0).at("running").get_boolean());
    EXPECT_EQ(host->disconnect(host->context), 0);
}
TEST_F(GameApiRuntime, StoreSurvivesRestartAndKeepsOwnersSeparate)
{
    install(PLUGIN_GAME_PATH, true);
    ASSERT_NE(host, nullptr);
    auto other = Symbol<const NcHost*(NC_CALL*)()>(L"game-other.dll", "nc_test_host")();
    EXPECT_EQ(host->store_set(host->context, "same", R"({"text":"saved","list":[1,true,null]})"), 1);
    EXPECT_EQ(host->store_set(host->context, "null", "null"), 1);
    EXPECT_EQ(other->store_get(other->context, "same", nullptr, 0), 0u);
    EXPECT_EQ(other->store_set(other->context, "same", "42"), 1);
    restart();
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(parse(get("same")).at("text"), "saved");
    EXPECT_EQ(get("null"), "null");
    EXPECT_EQ(get("absent"), "");
    char small[2]{'x', 'x'};
    EXPECT_GT(host->store_get(host->context, "same", small, sizeof(small)), sizeof(small));
    EXPECT_EQ(small[0], '\0');
    char keys[128]{};
    EXPECT_GT(host->store_keys(host->context, keys, sizeof(keys)), 0u);
    EXPECT_EQ(parse(keys), Json::array({"null", "same"}));
    EXPECT_EQ(host->store_delete(host->context, "same"), 1);
    EXPECT_EQ(host->store_delete(host->context, "missing"), 1);
    restart();
    EXPECT_EQ(get("same"), "");
    std::ifstream file(dir / L"plugins/data/plugin-test.other.json");
    EXPECT_EQ(parse(std::string(std::istreambuf_iterator<char>(file), {})).at("same"), 42);
}
TEST_F(GameApiRuntime, StoreRejectsInvalidValuesAndAtomicFailurePreservesPreviousData)
{
    install(PLUGIN_GAME_PATH);
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(host->store_set(host->context, "value", "1"), 1);
    for (const auto* key : {"", "../other", "a/b", "a\\b", "a:b"})
        EXPECT_EQ(host->store_set(host->context, key, "2"), 0);
    EXPECT_EQ(host->store_set(host->context, "value", "{broken}"), 0);
    EXPECT_EQ(host->store_set(host->context, "value", ("\"" + std::string(65536, 'x') + "\"").c_str()), 0);
    const auto file = dir / L"plugins/data/plugin-test.game.json";
    HANDLE locked = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(locked, INVALID_HANDLE_VALUE);
    EXPECT_EQ(host->store_set(host->context, "value", "2"), 0);
    EXPECT_EQ(get("value"), "1");
    CloseHandle(locked);
    restart();
    EXPECT_EQ(get("value"), "1");
    // Stored objects count toward the same JSON depth limit as their values.
    EXPECT_EQ(host->store_set(host->context, "deep", (std::string(16, '[') + "0" + std::string(16, ']')).c_str()), 0);
    EXPECT_EQ(host->store_set(host->context, "nested", (std::string(8, '[') + "0" + std::string(8, ']')).c_str()), 1);
    restart();
    EXPECT_FALSE(get("nested").empty());
}
TEST_F(GameApiRuntime, WorkerThreadsAndUnavailableServicesCannotAccessEngine)
{
    install(PLUGIN_GAME_ALL_PATH);
    ASSERT_NE(host, nullptr);
    std::thread worker([&] {
        EXPECT_EQ(host->game_data(host->context, "connection", 0, nullptr, 0), 0u);
        EXPECT_EQ(host->store_set(host->context, "thread", "1"), 0);
        EXPECT_EQ(host->subscribe_event(host->context, "player.health", 1), 0);
        EXPECT_EQ(host->send_chat(host->context, "hello", 0), 0);
        EXPECT_EQ(host->connect(host->context, "localhost", 27015), 0);
        EXPECT_EQ(host->disconnect(host->context), 0);
    });
    worker.join();
    EXPECT_TRUE(get("thread").empty());
    nc_runtime_bind_client(nullptr);
    EXPECT_EQ(host->game_data(host->context, "connection", 0, nullptr, 0), 0u);
    EXPECT_EQ(host->send_chat(host->context, "hello", 0), 0);
    EXPECT_EQ(host->connect(host->context, "localhost", 27015), 0);
    EXPECT_EQ(host->disconnect(host->context), 0);
    EXPECT_EQ(host->store_set(host->context, "offline", "true"), 1);
}
TEST_F(GameApiRuntime, CorruptStoreIsPreservedAndNeverReplacedWithDefaults)
{
    install(PLUGIN_GAME_PATH);
    const auto file = dir / L"plugins/data/plugin-test.game.json";
    fs::create_directories(file.parent_path());
    {
        std::ofstream stream(file);
        stream << "{corrupt}";
    }
    EXPECT_EQ(host->store_get(host->context, "key", nullptr, 0), 0u);
    EXPECT_EQ(host->store_set(host->context, "key", "1"), 0);
    EXPECT_EQ(host->store_delete(host->context, "key"), 0);
    std::ifstream stream(file);
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(stream), {}), "{corrupt}");
}
TEST_F(GameApiRuntime, StoreLimitsAllowExistingKeyUpdatesButRejectOverflow)
{
    install(PLUGIN_GAME_PATH);
    const auto file = dir / L"plugins/data/plugin-test.game.json";
    fs::create_directories(file.parent_path());
    Json data = tao::json::empty_object;
    for (int i = 0; i < 1024; ++i)
        data["k" + std::to_string(i)] = 0;
    {
        std::ofstream stream(file);
        stream << tao::json::to_string(data);
    }
    EXPECT_EQ(host->store_set(host->context, "overflow", "1"), 0);
    EXPECT_EQ(host->store_set(host->context, "k0", "1"), 1);
    EXPECT_EQ(get("k0"), "1");
    nc_runtime_stop();
    data = tao::json::empty_object;
    for (int i = 0; i < 16; ++i)
        data["k" + std::to_string(i)] = std::string(65500, 'a');
    {
        std::ofstream stream(file);
        stream << tao::json::to_string(data);
    }
    restart();
    const auto value = tao::json::to_string(Json(std::string(65500, 'b')));
    EXPECT_EQ(host->store_set(host->context, "overflow", value.c_str()), 0);
    EXPECT_EQ(host->store_set(host->context, "k0", "1"), 1);
    EXPECT_EQ(get("k0"), "1");
}
TEST_F(GameApiRuntime, RustEventsAndPersistenceUseSameAbi)
{
    const auto path = _wgetenv(L"NEXTCLIENT_RUST_EVENTS_DLL");
    if (!path)
        GTEST_SKIP() << "Set NEXTCLIENT_RUST_EVENTS_DLL to exercise the Rust events example";
    fs::copy_file(path, dir / L"plugins/rust-events.dll");
    nc_runtime_start(dir.c_str(), 0);
    auto plugins = rows();
    ASSERT_EQ(plugins.get_array().size(), 1u);
    plugins.at(0)["enabled"] = plugins.at(0)["consent"] = true;
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(plugins).c_str()), "");
    restart();
    ASSERT_TRUE(rows().at(0).at("running").get_boolean());
    nc_runtime_event("player.health", R"({"health":73})");
    frame();
    restart();
    std::ifstream stream(dir / L"plugins/data/plugin-org.nextclient.events.json");
    const auto store = parse(std::string(std::istreambuf_iterator<char>(stream), {}));
    EXPECT_EQ(store.at("last_health").at("health"), 73);
    EXPECT_EQ(store.at("starts"), 2);
}
TEST_F(GameApiRuntime, LocalChatIsIndependentOfNetworkChatPermissions)
{
    install(PLUGIN_GAME_PRINT_PATH);
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(host->chat_print(host->context, "local ; say text"), 1);
    EXPECT_EQ(local_chat, (std::vector<std::string>{"local ; say text"}));
    EXPECT_TRUE(sent.empty());
    EXPECT_EQ(host->send_chat(host->context, "network", 0), 0);
    EXPECT_EQ(host->subscribe_event(host->context, "chat.message", 1), 0);
    EXPECT_EQ(host->chat_print(host->context, nullptr), 0);
    EXPECT_EQ(host->chat_print(host->context, ""), 0);
    EXPECT_EQ(host->chat_print(host->context, "line\nbreak"), 0);
    EXPECT_EQ(host->chat_print(host->context, "\x04[Life Stats]\x01 summary"), 1);
    EXPECT_EQ(local_chat.back(), "\x04[Life Stats]\x01 summary");
    EXPECT_EQ(host->chat_print(host->context, "\x02unsupported"), 0);
    EXPECT_EQ(host->chat_print(host->context, "\x1b[31mterminal escape"), 0);
    EXPECT_EQ(host->chat_print(host->context, std::string(191, 'x').c_str()), 0);
    std::thread worker([&] { EXPECT_EQ(host->chat_print(host->context, "worker"), 0); });
    worker.join();
    nc_runtime_bind_client(nullptr);
    EXPECT_EQ(host->chat_print(host->context, "unavailable"), 0);
    EXPECT_EQ(local_chat.size(), 2u);
}
TEST_F(GameApiRuntime, NetworkChatPermissionDoesNotAllowLocalHudOutput)
{
    install(PLUGIN_GAME_ALL_PATH);
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(host->chat_print(host->context, "not granted"), 0);
    EXPECT_TRUE(local_chat.empty());
}

TEST_F(GameApiRuntime, LongestNamesCanBeCreatedReadWrittenAndWatched)
{
    // Noncapturing service probes verify that validation reaches the engine.
    static std::string read_name, write_name;
    services.read_cvar = [](const char* name, char* out, uint32_t capacity) -> uint32_t {
        read_name = name;
        if (out && capacity >= 2)
            std::memcpy(out, "0", 2);
        return 2;
    };
    services.write_cvar = [](const char* name, const char* value) -> int32_t {
        write_name = name;
        nc_runtime_cvar_changed(name, "0", value);
        return 1;
    };
    nc_runtime_bind_client(&services);
    install(PLUGIN_GAME_LONG_PATH);
    ASSERT_NE(host, nullptr);
    ASSERT_EQ(creation(), 1);
    const auto generated = "nc." + std::string(96, 'a') + "." + std::string(48, 'b');
    ASSERT_EQ(generated.size(), 148u);
    ASSERT_EQ(created, (std::vector<std::string>{generated + "=0:1"}));
    for (const auto& name : {generated, std::string(160, 'z')})
    {
        EXPECT_EQ(host->watch_cvar(host->context, name.c_str(), 1), 1);
        char value[2]{};
        EXPECT_EQ(host->read_cvar(host->context, name.c_str(), value, sizeof(value)), 2u);
        EXPECT_EQ(read_name, name);
        EXPECT_STREQ(value, "0");
        EXPECT_EQ(host->write_cvar(host->context, name.c_str(), "1"), 1);
        EXPECT_EQ(write_name, name);
    }
    frame();
    ASSERT_EQ(count(), 2);
    EXPECT_EQ(payload(0).at("name"), generated);
    EXPECT_EQ(payload(1).at("name"), std::string(160, 'z'));
    const auto too_long = std::string(161, 'z');
    read_name.clear();
    write_name.clear();
    EXPECT_EQ(host->read_cvar(host->context, too_long.c_str(), nullptr, 0), 0u);
    EXPECT_EQ(host->write_cvar(host->context, too_long.c_str(), "2"), 0);
    EXPECT_EQ(host->watch_cvar(host->context, too_long.c_str(), 1), 0);
    nc_runtime_cvar_changed(too_long.c_str(), "1", "2");
    frame();
    EXPECT_EQ(count(), 2);
    EXPECT_TRUE(read_name.empty());
    EXPECT_TRUE(write_name.empty());
}
