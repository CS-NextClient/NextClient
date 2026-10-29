#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "engine.h"
#include "client/ClientStringRegistry.h"
#include "client/cl_string_registry.h"

namespace
{
    goldsrc::pfnUserMsgHook g_StringMessage{};
    std::vector<std::string> g_DisconnectEvents;

    int HookMessage(const char* name, goldsrc::pfnUserMsgHook callback)
    {
        EXPECT_STREQ(name, "ncl_str_reg");
        g_StringMessage = callback;
        return 1;
    }

    int AddCommand(const char* name, goldsrc::xcommand_t)
    {
        EXPECT_STREQ(name, "print_ncl_strings");
        return 1;
    }

    void SendString(uint16_t id, const char* value)
    {
        std::vector<uint8_t> bytes = {static_cast<uint8_t>(id), static_cast<uint8_t>(id >> 8)};
        const std::string text(value);
        bytes.insert(bytes.end(), text.begin(), text.end());
        bytes.push_back(0);
        ASSERT_NE(g_StringMessage, nullptr);
        EXPECT_EQ(g_StringMessage("ncl_str_reg", static_cast<int>(bytes.size()), bytes.data()), 1);
    }
} // namespace

void COM_ExplainDisconnection(goldsrc::qboolean print, const char* format, ...)
{
    EXPECT_TRUE(print);
    EXPECT_STREQ(format, "Server exceeded client string cache limit.");
    g_DisconnectEvents.push_back("explain");
}

void CL_Disconnect()
{
    g_DisconnectEvents.push_back("disconnect");
    CL_StringRegistryClear();
}

TEST(ClientStringRegistryIntegrationTest, MessageAdapterUsesTheRootOwnerAndExplainsBeforeDisconnect)
{
    const goldsrc::cl_enginefunc_t saved = gEngfuncs;
    gEngfuncs.pfnHookUserMsg = HookMessage;
    gEngfuncs.pfnAddCommand = AddCommand;
    CL_StringRegistryClear();
    g_DisconnectEvents.clear();
    CL_StringRegistryInit();
    SendString(7, "first");
    EXPECT_STREQ(CL_GetStringRegistry().Find(7), "first");
    EXPECT_EQ(CL_StringRegistryGetString(7), CL_GetStringRegistry().Find(7));
    CL_StringRegistryClear();
    EXPECT_EQ(CL_StringRegistryGetString(7), nullptr);
    for (uint16_t id = 1; id <= 2048; id++)
    {
        SendString(id, "value");
    }
    SendString(7, "replacement");
    EXPECT_STREQ(CL_StringRegistryGetString(7), "replacement");
    EXPECT_TRUE(g_DisconnectEvents.empty());
    SendString(2049, "overflow");
    EXPECT_EQ(g_DisconnectEvents, (std::vector<std::string>{"explain", "disconnect"}));
    EXPECT_EQ(CL_GetStringRegistry().Find(7), nullptr);
    CL_StringRegistryShutdown();
    gEngfuncs = saved;
}
