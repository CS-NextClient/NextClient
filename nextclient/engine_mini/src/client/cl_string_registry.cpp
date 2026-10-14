#include "engine.h"
#include "console/console.h"
#include "client/cl_main.h"

#include <goldsrc/client_api.h>
#include <data_types/MessageReader.h>
#include <unordered_map>

namespace
{
    constexpr size_t kMaxCacheSize = 2048;

    std::unordered_map<uint16_t, std::string> g_Entries;
    int g_MsgNclStrReg = 0;

    void RegisterString(uint16_t string_id, std::string value)
    {
        if (string_id == 0)
        {
            return;
        }

        if (g_Entries.size() >= kMaxCacheSize && !g_Entries.count(string_id))
        {
            COM_ExplainDisconnection(true, "Server exceeded client string cache limit.");
            CL_Disconnect();
            return;
        }

        g_Entries[string_id] = std::move(value);
    }

    int MsgFunc_NclStrReg(const char* msg_name, int size, void* buf)
    {
        MessageReader reader(buf, size);

        uint16_t string_id = static_cast<uint16_t>(reader.ReadShort());
        if (reader.is_overflowed())
        {
            return 1;
        }

        std::string value = reader.ReadString();
        if (reader.is_overflowed())
        {
            return 1;
        }

        RegisterString(string_id, value);
        return 1;
    }

    void PrintNclStrings()
    {
        Con_Printf("NCL strings:\n");

        for (const auto& entry : g_Entries)
        {
            Con_Printf("%d: %s\n", entry.first, entry.second.c_str());
        }
    }
} // namespace


void CL_StringRegistryInit()
{
    g_MsgNclStrReg = gEngfuncs.pfnHookUserMsg("ncl_str_reg", MsgFunc_NclStrReg);
    gEngfuncs.pfnAddCommand("print_ncl_strings", PrintNclStrings);
}

void CL_StringRegistryShutdown()
{
    g_Entries.clear();
    g_MsgNclStrReg = 0;
}

void CL_StringRegistryClear()
{
    g_Entries.clear();
}

const char* CL_StringRegistryGetString(uint16_t string_id)
{
    auto it = g_Entries.find(string_id);
    if (it != g_Entries.end())
    {
        return it->second.c_str();
    }

    return nullptr;
}
