#include "RadarHooks.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "main.h"

#include <next_client_mini/client_mini.h>
#include <data_types/MessageReader.h>
#include <goldsrc/client_api.h>
#include <goldsrc/event_api.h>

#include "RadarBroadcast.h"
#include "HudTextBuffer.h"
#include "SmokeRegistry.h"

namespace
{
    HudTextBuffer g_TextBuffer;
    goldsrc::cl_enginefunc_t g_OriginalTextTable;
    goldsrc::cl_enginefunc_t* g_PatchedEngTable;

    void SetEngineTextColor(int r, int g, int b)
    {
        g_NitroApi->GetEngineData()->VGUI2_Draw_SetTextColor(r, g, b);
    }

    void VGUI2_Draw_SetTextColorHandler(int r, int g, int b, nitroapi::NextHandlerInterface<void, int, int, int>* next)
    {
        g_TextBuffer.SetColor(r, g, b);
        next->Invoke(r, g, b);
    }

    int Hooked_DrawConsoleString(int x, int y, char* string)
    {
        return g_TextBuffer.DrawConsoleString(x, y, string);
    }

    int Hooked_DrawString(int x, int y, const char* string, int r, int g, int b)
    {
        return g_TextBuffer.DrawString(x, y, string, r, g, b);
    }

    int Hooked_DrawStringReverse(int x, int y, const char* string, int r, int g, int b)
    {
        return g_TextBuffer.DrawStringReverse(x, y, string, r, g, b);
    }

    int Hooked_DrawCharacter(int x, int y, int ch, int r, int g, int b)
    {
        return g_TextBuffer.DrawCharacter(x, y, ch, r, g, b);
    }

    int Hooked_VGUI2DrawCharacter(int x, int y, int ch, unsigned int font)
    {
        return g_TextBuffer.VGUI2DrawCharacter(x, y, ch, font);
    }

    int Hooked_VGUI2DrawCharacterAdd(int x, int y, int ch, int r, int g, int b, unsigned int font)
    {
        return g_TextBuffer.VGUI2DrawCharacterAdd(x, y, ch, r, g, b, font);
    }

    RadarSinks g_Sinks;
    void (*g_OrigCreatesmoke)(goldsrc::event_args_t*);

    bool IsNonZero(const ncl_math::Vector3& v)
    {
        return std::fabs(v.x) > 1.0f || std::fabs(v.y) > 1.0f || std::fabs(v.z) > 1.0f;
    }

    void CreateSmoke_Hook(goldsrc::event_args_t* args)
    {
        // The exact centre rides in angles for the per-second puffs and in origin for the
        // detonation, which is the event with iparam2 == 1.
        if (g_Sinks.smoke)
        {
            bool has_angles = IsNonZero(ncl_math::Vector3::Ref(args->angles));
            const float* exact = has_angles ? args->angles : args->origin;
            bool detonation = (args->iparam2 == 1);

            g_Sinks.smoke->OnSmoke(ncl_math::Vector3::Ref(exact), has_angles || detonation, gEngfuncs.GetClientTime());
        }

        if (g_OrigCreatesmoke)
        {
            g_OrigCreatesmoke(args);
        }
    }

    void CL_HookEventHandler(char* name, void (*cb)(goldsrc::event_args_t*), CL_HookEventNext next)
    {
        if (name && std::strstr(name, "createsmoke"))
        {
            g_OrigCreatesmoke = cb;
            next->Invoke(name, CreateSmoke_Hook);
            return;
        }

        next->Invoke(name, cb);
    }

    int UserMsg_BombDropHandler(const char* name, int size, void* data, UserMsg_RadarMsgNext next)
    {
        MessageReader reader(data, size);
        float x = reader.ReadCoord();
        float y = reader.ReadCoord();
        float z = reader.ReadCoord();
        int planted = reader.ReadByte();

        ncl_math::Vector3 pos{x, y, z};
        float time = gEngfuncs.GetClientTime();

        if (g_Sinks.broadcast)
        {
            if (pos == ncl_math::Vector3{})
            {
                g_Sinks.broadcast->ClearBomb(time); // engine "hide" sentinel
            }
            else
            {
                g_Sinks.broadcast->SetBomb(pos, planted != 0, time);
            }
        }

        return next->Invoke(name, size, data);
    }

    int UserMsg_BombPickupHandler(const char* name, int size, void* data, UserMsg_RadarMsgNext next)
    {
        if (g_Sinks.broadcast)
        {
            g_Sinks.broadcast->ClearBomb(gEngfuncs.GetClientTime());
        }

        return next->Invoke(name, size, data);
    }

    int UserMsg_HostagePosHandler(const char* name, int size, void* data, UserMsg_RadarMsgNext next)
    {
        MessageReader reader(data, size);
        reader.ReadByte(); // 1 on the batch a spawning player gets, 0 on the periodic updates
        int index = reader.ReadByte();
        float x = reader.ReadCoord();
        float y = reader.ReadCoord();
        reader.ReadCoord(); // z, unused on the 2D radar

        if (g_Sinks.broadcast)
        {
            g_Sinks.broadcast->SetHostage(index, {x, y}, gEngfuncs.GetClientTime());
        }

        return next->Invoke(name, size, data);
    }

    int UserMsg_HostageKHandler(const char* name, int size, void* data, UserMsg_RadarMsgNext next)
    {
        MessageReader reader(data, size);
        int index = reader.ReadByte();

        if (g_Sinks.broadcast)
        {
            g_Sinks.broadcast->KillHostage(index);
        }

        return next->Invoke(name, size, data);
    }
} // namespace

// Patches client.dll's own copy of the engine table: the stock client copies the engine
// functions into it in Initialize and calls them through the copy, so patching the engine's
// table would not reach it. NextClient's HUD text uses the engine table and stays unshifted.
void RadarHooks_PatchHudText()
{
    goldsrc::cl_enginefunc_t* ef = g_NitroApi->GetClientData()->gEngfuncs;

    if (!ef || g_PatchedEngTable)
    {
        return;
    }

    g_OriginalTextTable = *ef;
    g_TextBuffer.Initialize(*ef, SetEngineTextColor);
    ef->pfnDrawConsoleString = Hooked_DrawConsoleString;
    ef->pfnDrawString = Hooked_DrawString;
    ef->pfnDrawStringReverse = Hooked_DrawStringReverse;
    ef->pfnDrawCharacter = Hooked_DrawCharacter;
    ef->pfnVGUI2DrawCharacter = Hooked_VGUI2DrawCharacter;
    ef->pfnVGUI2DrawCharacterAdd = Hooked_VGUI2DrawCharacterAdd;
    g_PatchedEngTable = ef;
}

void RadarHooks_UnpatchHudText()
{
    if (g_PatchedEngTable)
    {
        g_PatchedEngTable->pfnDrawConsoleString = g_OriginalTextTable.pfnDrawConsoleString;
        g_PatchedEngTable->pfnDrawString = g_OriginalTextTable.pfnDrawString;
        g_PatchedEngTable->pfnDrawStringReverse = g_OriginalTextTable.pfnDrawStringReverse;
        g_PatchedEngTable->pfnDrawCharacter = g_OriginalTextTable.pfnDrawCharacter;
        g_PatchedEngTable->pfnVGUI2DrawCharacter = g_OriginalTextTable.pfnVGUI2DrawCharacter;
        g_PatchedEngTable->pfnVGUI2DrawCharacterAdd = g_OriginalTextTable.pfnVGUI2DrawCharacterAdd;
    }

    g_TextBuffer.Reset();
    g_PatchedEngTable = nullptr;
}

void RadarHooks_SetSinks(const RadarSinks& sinks)
{
    g_Sinks = sinks;
}

void RadarHooks_ClearSinks()
{
    g_Sinks = RadarSinks{};
}

void RadarHooks_FlushText()
{
    g_TextBuffer.Flush();
}

void RadarHooks_InvalidateGlyphMetrics()
{
    g_TextBuffer.InvalidateGlyphMetrics();
}

void RadarHooks_Subscribe(nitroapi::NitroApiInterface* nitro_api, std::vector<std::shared_ptr<nitroapi::Unsubscriber>>& unsub)
{
    nitroapi::ClientData* client_data = nitro_api->GetClientData();
    unsub.emplace_back(nitro_api->GetEngineData()->VGUI2_Draw_SetTextColor |= VGUI2_Draw_SetTextColorHandler);
    unsub.emplace_back(nitro_api->GetEngineData()->CL_HookEvent |= CL_HookEventHandler);
    unsub.emplace_back(client_data->UserMsg_BombDrop |= UserMsg_BombDropHandler);
    unsub.emplace_back(client_data->UserMsg_BombPickup |= UserMsg_BombPickupHandler);
    unsub.emplace_back(client_data->UserMsg_HostagePos |= UserMsg_HostagePosHandler);
    unsub.emplace_back(client_data->UserMsg_HostageK |= UserMsg_HostageKHandler);
}
