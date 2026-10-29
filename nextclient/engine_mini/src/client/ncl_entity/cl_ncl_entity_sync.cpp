#include "engine.h"
#include "cl_ncl_entity_sync.h"

#include <vector>
#include <string>

#include <tier0/basetypes.h>
#include <goldsrc/client_api.h>
#include <data_types/MessageReader.h>

#include "console/console.h"

namespace
{
    int g_MsgNsyncCreate;
    int g_MsgNsyncUpdate;
    int g_MsgNsyncDestroy;

    std::array<std::unique_ptr<INclEntityHandler>, 8> g_Handlers;

    bool IsTypeValid(uint8_t type_id)
    {
        return type_id > 0 && type_id < std::size(g_Handlers) && g_Handlers[type_id] != nullptr;
    }

    int MsgFunc_EntitySyncCreate(const char* msg_name, int size, void* buf)
    {
        MessageReader reader(buf, size);

        uint8_t type_id = reader.ReadByte();
        if (reader.is_overflowed())
        {
            return 1;
        }

        uint16_t entity_id = static_cast<uint16_t>(reader.ReadShort());
        if (reader.is_overflowed())
        {
            return 1;
        }

        if (!IsTypeValid(type_id))
        {
            return 1;
        }

        NclEntityFields fields;
        if (!NclEntityMessage::ReadFields(reader, g_Handlers[type_id]->Fields(), false, 0, fields))
        {
            return 1;
        }

        g_Handlers[type_id]->OnCreate(entity_id, fields);

        return 1;
    }

    int MsgFunc_EntitySyncUpdate(const char* msg_name, int size, void* buf)
    {
        MessageReader reader(buf, size);

        uint8_t type_id = reader.ReadByte();
        if (reader.is_overflowed())
        {
            return 1;
        }

        uint16_t entity_id = static_cast<uint16_t>(reader.ReadShort());
        if (reader.is_overflowed())
        {
            return 1;
        }

        uint16_t field_mask = static_cast<uint16_t>(reader.ReadShort());
        if (reader.is_overflowed())
        {
            return 1;
        }

        if (!IsTypeValid(type_id))
        {
            return 1;
        }

        NclEntityFields fields;
        if (!NclEntityMessage::ReadFields(reader, g_Handlers[type_id]->Fields(), true, field_mask, fields))
        {
            return 1;
        }

        g_Handlers[type_id]->OnUpdate(entity_id, field_mask, fields);

        return 1;
    }

    int MsgFunc_EntitySyncDestroy(const char* msg_name, int size, void* buf)
    {
        MessageReader reader(buf, size);

        uint8_t type_id = reader.ReadByte();
        if (reader.is_overflowed())
        {
            return 1;
        }

        uint16_t entity_id = static_cast<uint16_t>(reader.ReadShort());
        if (reader.is_overflowed())
        {
            return 1;
        }

        if (!IsTypeValid(type_id))
        {
            return 1;
        }

        g_Handlers[type_id]->OnDestroy(entity_id);

        return 1;
    }

    void RegisterMessages()
    {
        g_MsgNsyncCreate = gEngfuncs.pfnHookUserMsg("nsync_create", MsgFunc_EntitySyncCreate);
        g_MsgNsyncUpdate = gEngfuncs.pfnHookUserMsg("nsync_update", MsgFunc_EntitySyncUpdate);
        g_MsgNsyncDestroy = gEngfuncs.pfnHookUserMsg("nsync_destroy", MsgFunc_EntitySyncDestroy);
    }
} // namespace

void CL_NclEntitySyncInit()
{
    RegisterMessages();
}

void CL_NclEntitySyncShutdown()
{
    for (std::unique_ptr<INclEntityHandler>& handler : g_Handlers)
    {
        if (handler)
        {
            handler->OnClear();
        }

        handler = nullptr;
    }

    g_MsgNsyncCreate = 0;
    g_MsgNsyncUpdate = 0;
    g_MsgNsyncDestroy = 0;
}

void CL_NclEntitySyncClear()
{
    for (std::unique_ptr<INclEntityHandler>& handler : g_Handlers)
    {
        if (handler != nullptr)
        {
            handler->OnClear();
        }
    }
}

void CL_NclEntitySyncRegisterType(uint8_t type_id, std::unique_ptr<INclEntityHandler> handler)
{
    if (type_id >= std::size(g_Handlers) || g_Handlers[type_id] != nullptr)
    {
        Con_DPrintf(ConLogType::Warning, "CL_NclEntitySyncRegisterType: invalid type_id=%d\n", type_id);
        return;
    }

    g_Handlers[type_id] = std::move(handler);
}

INclEntityHandler* CL_NclEntitySyncGetHandler(uint8_t type_id)
{
    if (!IsTypeValid(type_id))
    {
        return nullptr;
    }

    return g_Handlers[type_id].get();
}

void CL_NclEntitySyncCollectDebugInfo(std::vector<std::string>& out_lines)
{
    for (size_t i = 0; i < std::size(g_Handlers); ++i)
    {
        std::unique_ptr<INclEntityHandler>& handler = g_Handlers[i];

        if (handler == nullptr)
        {
            continue;
        }

        handler->CollectDebugInfo(out_lines);
    }
}
