#include <gtest/gtest.h>
#include <nextclient/runtime.h>
#include <tao/json.hpp>
#include <filesystem>
#include <cstdarg>
#include <limits>
#include <map>
#include "main.h"
#include "plugin_movement.h"
#include "plugin_bridge.h"
#include "color_chat_in_console.h"

// Exercise the real bridge and runtime with the partial HUD exposed by Nitro.
// GameApi's exported host calls the same SDK entry points as a loaded plugin.
cl_enginefunc_t gEngfuncs{};
nitroapi::NitroApiInterface* g_NitroApi{};
local_state_t g_LastPlayerState{};
client_data_t g_LastClientData{};
gamehud_t* gHUD{};
playermove_t* pmove{};
IGameConsole* g_GameConsole{};
IGameConsoleNext* g_GameConsoleNext{};

nitroapi::EngineData* eng()
{
    static nitroapi::EngineData data;
    return &data;
}
nitroapi::ClientData* client()
{
    static nitroapi::ClientData data;
    return &data;
}
// The stock client supplies these methods in-game. This fixture only reads HUD data.
int CHudHealth::Init()
{
    return 0;
}
int CHudHealth::VidInit()
{
    return 0;
}
int CHudHealth::Draw(float)
{
    return 0;
}
void CHudHealth::Reset() {}

namespace
{
    client_static_t connection{};
    cl_entity_t local{};
    model_t model{};
    playermove_t movement{};
    gamehud_t hud{};
    CHudHealth health{};
    int weapons = 1 << 7;
    std::vector<std::string> printed;
    std::vector<char> chat_packet;
    std::map<std::string, pfnUserMsgHook> message_hooks;
    int hud_message_calls{};
    int HookMessage(const char* name, pfnUserMsgHook handler)
    {
        message_hooks[name] = handler;
        return 1;
    }
    void ConsolePrintf(const char* format, ...)
    {
        char text[512];
        va_list args;
        va_start(args, format);
        vsnprintf(text, sizeof(text), format, args);
        va_end(args);
        printed.emplace_back(text);
    }
    void ConsolePrint(const char* text)
    {
        printed.emplace_back(text);
    }
    int SayText(const char*, int size, void* data)
    {
        chat_packet.assign(static_cast<char*>(data), static_cast<char*>(data) + size);
        client()->gEngfuncs->pfnConsolePrint("plain echo");
        client()->gEngfuncs->Con_Printf("%s", "formatted echo");
        client()->gEngfuncs->Con_DPrintf("%s", "debug echo");
        return 1;
    }
} // namespace

class PluginBridge : public ::testing::Test
{
protected:
    std::filesystem::path dir;
    const NcHost* host{};
    virtual const char* fixture()
    {
        return PLUGIN_GAME_PATH;
    }

    void SetUp() override
    {
        dir = std::filesystem::temp_directory_path() /
              (L"plugin-bridge-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        std::filesystem::create_directories(dir / L"plugins");
        std::filesystem::copy_file(fixture(), dir / L"plugins/game-api.dll");
        eng()->client_static = &connection;
        connection.state = ca_active;
        local.index = 1;
        local.model = &model;
        gEngfuncs.GetLocalPlayer = [] { return &local; };
        gEngfuncs.GetViewAngles = [](float* angles) { angles[0] = angles[1] = angles[2] = 0; };
        gEngfuncs.pfnGetScreenInfo = [](SCREENINFO*) { return 1; };
        gEngfuncs.GetMaxClients = [] { return 0; };
        gEngfuncs.GetClientTime = [] { return 10.0f; };
        gEngfuncs.pfnGetLevelName = [] { return "test"; };
        gEngfuncs.ServerInfo_ValueForKey = [](const char*) { return ""; };
        gEngfuncs.pfnHookUserMsg = HookMessage;
        message_hooks.clear();
        hud_message_calls = 0;
        PluginBridgeWrapMessages();
        hud.m_Health = &health;
        hud.m_Health->m_iHealth = 100;
        hud.m_iWeaponBits = &weapons;
        ASSERT_EQ(hud.m_Battery, nullptr); // Deliberately unbound in ClientModule.
        gHUD = &hud;
        pmove = &movement;
        PluginBridgeInit();
        nc_runtime_start(dir.c_str(), 0);
        auto rows = tao::json::from_string(nc_runtime_catalog()).at("plugins");
        ASSERT_EQ(rows.get_array().size(), 1u);
        rows.at(0)["enabled"] = rows.at(0)["consent"] = true;
        ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
        nc_runtime_stop();
        nc_runtime_start(dir.c_str(), 0);
        auto get_host = reinterpret_cast<const NcHost*(NC_CALL*)()>(GetProcAddress(GetModuleHandleW(L"game-api.dll"), "nc_test_host"));
        ASSERT_NE(get_host, nullptr);
        host = get_host();
        ASSERT_NE(host, nullptr);
    }

    void TearDown() override
    {
        nc_runtime_stop();
        PluginBridgeShutdown();
        std::filesystem::remove_all(dir);
        gHUD = nullptr;
        pmove = nullptr;
        eng()->client_static = nullptr;
    }
};
class PluginMessageBridge : public PluginBridge
{
protected:
    const char* fixture() override
    {
        return PLUGIN_MESSAGE_PATH;
    }
};
TEST_F(PluginMessageBridge, CustomSubscriptionPreservesEarlierAndLaterHandlers)
{
    auto earlier = +[](const char*, int, void*) {
        ++hud_message_calls;
        return 17;
    };
    auto later = +[](const char*, int, void*) {
        ++hud_message_calls;
        return 23;
    };
    gEngfuncs.pfnHookUserMsg("Custom", earlier);
    const auto* api = host->query_interface(host->context, "nextclient.messages", 1);
    ASSERT_NE(api, nullptr);
    const auto result = api->call(host->context, "subscribe", R"({"name":"Custom"})");
    char output[256]{};
    api->read_result(host->context, result, output, sizeof(output));
    api->release_result(host->context, result);
    ASSERT_TRUE(tao::json::from_string(output).at("ok").get_boolean());
    char bytes[]{1, 2, 3};
    EXPECT_EQ(message_hooks.at("Custom")("Custom", sizeof(bytes), bytes), 17);
    EXPECT_EQ(hud_message_calls, 1);
    gEngfuncs.pfnHookUserMsg("Custom", later);
    EXPECT_EQ(message_hooks.at("Custom")("Custom", sizeof(bytes), bytes), 23);
    EXPECT_EQ(hud_message_calls, 2);
    PluginBridgeShutdown();
    EXPECT_EQ(message_hooks.at("Custom"), later);
}
TEST_F(PluginMessageBridge, PresentationHideSkipsOriginalWithoutBlockingFutureRegistrations)
{
    auto original = +[](const char*, int, void*) {
        ++hud_message_calls;
        return 17;
    };
    gEngfuncs.pfnHookUserMsg("ScreenShake", original);
    const auto* api = host->query_interface(host->context, "nextclient.messages", 1);
    ASSERT_NE(api, nullptr);
    auto hide = +[](void*, const char*, const uint8_t*, uint32_t, uint8_t*, uint32_t*) -> int32_t { return 2; };
    ASSERT_EQ(api->set_filter(host->context, "ScreenShake", hide, nullptr), 1);
    char bytes[6]{};
    EXPECT_EQ(message_hooks.at("ScreenShake")("ScreenShake", sizeof(bytes), bytes), 1);
    EXPECT_EQ(hud_message_calls, 0);
    api->set_filter(host->context, "ScreenShake", nullptr, nullptr);
    EXPECT_EQ(message_hooks.at("ScreenShake")("ScreenShake", sizeof(bytes), bytes), 17);
    EXPECT_EQ(hud_message_calls, 1);
}

TEST_F(PluginBridge, PlayerSnapshotOnJoinDoesNotRequireBatteryHud)
{
    NcPlayerState state{sizeof(NcPlayerState)};
    EXPECT_EQ(host->get_player(host->context, &state), 0);
    PluginBridgePredictionReady();
    ASSERT_EQ(host->get_player(host->context, &state), 1);
    EXPECT_EQ(state.index, 1);
    EXPECT_EQ(state.health, 100);
    EXPECT_EQ(state.armor, 0);
    EXPECT_EQ(state.weapons, static_cast<uint32_t>(weapons));
    PluginBridgeReset();
    EXPECT_EQ(host->get_player(host->context, &state), 0);
}

TEST_F(PluginBridge, FrameTimeIsEngineDeltaAtConstantFrameRate)
{
    for (int i = 0; i < 30; ++i)
    {
        PluginBridgeFrame(0.01);
        NcSession state{sizeof(NcSession)};
        ASSERT_EQ(host->get_session(host->context, &state), 1);
        EXPECT_FLOAT_EQ(state.frame_time, 0.01f);
    }
    for (double invalid : {-1.0, std::numeric_limits<double>::quiet_NaN()})
    {
        PluginBridgeFrame(invalid);
        NcSession state{sizeof(NcSession)};
        ASSERT_EQ(host->get_session(host->context, &state), 1);
        EXPECT_FLOAT_EQ(state.frame_time, 0);
    }
}

TEST_F(PluginBridge, OptionalScoreboardMessagesWorkWithoutHudHandlersAndPreserveLaterHandlers)
{
    gEngfuncs.GetMaxClients = [] { return 2; };
    gEngfuncs.pfnGetPlayerInfo = [](int, hud_player_info_t* info) {
        *info = {};
        static char name[] = "Alice";
        info->name = name;
    };
    ASSERT_NE(message_hooks.at("HealthInfo"), nullptr);
    ASSERT_NE(message_hooks.at("Account"), nullptr);
    unsigned char health_packet[]{2, 0xe8, 3, 0, 0}; // 1000 HP, not byte-truncated.
    unsigned char money_packet[]{2, 0x40, 0x1f, 0, 0};
    EXPECT_EQ(message_hooks.at("HealthInfo")("HealthInfo", sizeof(health_packet), health_packet), 1);
    EXPECT_EQ(message_hooks.at("Account")("Account", sizeof(money_packet), money_packet), 1);
    PluginBridgePredictionReady();
    auto row = [&] {
        char json[4096]{};
        const auto size = host->game_data(host->context, "scoreboard", 2, json, sizeof(json));
        EXPECT_GT(size, 0u);
        EXPECT_LE(size, sizeof(json));
        return tao::json::from_string(json);
    };
    EXPECT_EQ(row().at("reported_health"), 1000);
    EXPECT_EQ(row().at("reported_money"), 8000);
    EXPECT_TRUE(row().at("has_defuser").is_null());
    const auto consumer = +[](const char*, int, void*) {
        ++hud_message_calls;
        return 17;
    };
    gEngfuncs.pfnHookUserMsg("HealthInfo", consumer);
    PluginBridgeWrapMessages(); // Must not replace a real consumer with a fallback.
    unsigned char hidden[]{2, 255, 255, 255, 255};
    EXPECT_EQ(message_hooks.at("HealthInfo")("HealthInfo", sizeof(hidden), hidden), 17);
    EXPECT_EQ(hud_message_calls, 1);
    EXPECT_TRUE(row().at("reported_health").is_null());
    EXPECT_EQ(row().at("reported_money"), 8000);
    PluginBridgeShutdown();
    EXPECT_EQ(message_hooks.at("HealthInfo"), consumer);
    EXPECT_EQ(message_hooks.at("Account"), nullptr);
    EXPECT_EQ(gEngfuncs.pfnHookUserMsg, HookMessage);
}

TEST(PluginOutput, LocalChatSuppressesEveryConsoleEchoAndRestoresEngineFunctions)
{
    cl_enginefunc_t engine{};
    engine.pfnConsolePrint = ConsolePrint;
    engine.Con_Printf = engine.Con_DPrintf = ConsolePrintf;
    client()->gEngfuncs = &engine;
    printed.clear();
    ASSERT_EQ(PrintLocalChat(SayText, "\x04[Life Stats]\x01 Example"), 1);
    EXPECT_TRUE(printed.empty());
    ASSERT_GT(chat_packet.size(), 2u);
    EXPECT_EQ(chat_packet[0], 0);
    EXPECT_STREQ(chat_packet.data() + 1, "\x04[Life Stats]\x01 Example\n");
    EXPECT_EQ(engine.pfnConsolePrint, ConsolePrint);
    EXPECT_EQ(engine.Con_Printf, ConsolePrintf);
    EXPECT_EQ(engine.Con_DPrintf, ConsolePrintf);
    engine.Con_Printf("%s", "ordinary console output");
    EXPECT_EQ(printed, std::vector<std::string>{"ordinary console output"});
    client()->gEngfuncs = nullptr;
}

TEST(PluginOutput, ConsoleFallbackStripsColorBytesAndPrintsFormatCharactersLiterally)
{
    gEngfuncs.Con_Printf = ConsolePrintf;
    printed.clear();
    PrintPluginConsole("\x04[Life Stats]\x01 %s %n\n");
    EXPECT_EQ(printed, (std::vector<std::string>{"[Life Stats]", " %s %n\n"}));
}

TEST(PluginMovement, PredictionFlagsShareJumpEligibilityWithoutChoosingActivation)
{
    EXPECT_EQ(PluginMovementFlags(nullptr), 0u);
    playermove_t value{};
    value.movetype = MOVETYPE_WALK;
    value.onground = -1;
    EXPECT_EQ(PluginMovementFlags(&value), NC_PLAYER_VALID | NC_PLAYER_CAN_JUMP);
    value.onground = 0;
    value.oldbuttons = IN_JUMP;
    const auto base = NC_PLAYER_VALID | NC_PLAYER_GROUNDED | NC_PLAYER_JUMP_HELD;
    EXPECT_EQ(PluginMovementFlags(&value), base | NC_PLAYER_CAN_JUMP);
    const auto walking = value;
    for (int condition = 0; condition < 9; ++condition)
    {
        value = walking;
        switch (condition)
        {
            case 0:
                value.dead = 1;
                break;
            case 1:
                value.deadflag = 1;
                break;
            case 2:
                value.spectator = 1;
                break;
            case 3:
                value.iuser1 = 1;
                break;
            case 4:
                value.movetype = MOVETYPE_NOCLIP;
                break;
            case 5:
                value.waterlevel = 2;
                break;
            case 6:
                value.waterjumptime = 1;
                break;
            case 7:
                value.flags = FL_WATERJUMP;
                break;
            case 8:
                value.flags = FL_FROZEN;
                break;
        }
        EXPECT_EQ(PluginMovementFlags(&value), base) << condition;
    }
}
