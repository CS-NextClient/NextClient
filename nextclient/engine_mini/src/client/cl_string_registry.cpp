#include "client/cl_string_registry.h"

#include <utility>

#include <data_types/MessageReader.h>
#include <goldsrc/client_api.h>

#include "client/ClientStringRegistry.h"
#include "client/cl_main.h"
#include "console/console.h"
#include "engine.h"

namespace
{
    ClientStringRegistry g_Registry;
    int g_MsgNclStrReg = 0;

    void RegisterString(uint16_t string_id, std::string value)
    {
        if (g_Registry.Register(string_id, std::move(value)) == ClientStringRegistry::RegisterResult::CapacityExceeded)
        {
            COM_ExplainDisconnection(true, "Server exceeded client string cache limit.");
            CL_Disconnect();
        }
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

        RegisterString(string_id, std::move(value));
        return 1;
    }

    void PrintNclStrings()
    {
        Con_Printf("NCL strings:\n");

        for (const auto& entry : g_Registry.get_entries())
        {
            Con_Printf("%d: %s\n", entry.first, entry.second.c_str());
        }
    }
} // namespace


ClientStringRegistry& CL_GetStringRegistry()
{
    return g_Registry;
}

void CL_StringRegistryInit()
{
    g_MsgNclStrReg = gEngfuncs.pfnHookUserMsg("ncl_str_reg", MsgFunc_NclStrReg);
    gEngfuncs.pfnAddCommand("print_ncl_strings", PrintNclStrings);
}

void CL_StringRegistryShutdown()
{
    g_Registry.Clear();
    g_MsgNclStrReg = 0;
}

void CL_StringRegistryClear()
{
    g_Registry.Clear();
}

const char* CL_StringRegistryGetString(uint16_t string_id)
{
    return g_Registry.Find(string_id);
}
