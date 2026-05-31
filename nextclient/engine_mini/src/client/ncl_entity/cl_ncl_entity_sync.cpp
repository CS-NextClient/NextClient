#include "engine.h"
#include "cl_ncl_entity_sync.h"

#include <vector>
#include <string>

#include <tier0/basetypes.h>
#include <const.h>
#include <parsemsg.h>

#include "console/console.h"

namespace
{
    int g_MsgNsyncCreate;
    int g_MsgNsyncUpdate;
    int g_MsgNsyncDestroy;

    std::array<std::unique_ptr<INclEntityHandler>, 8> g_Handlers;

    ncl_entity::FieldValue ReadFieldValue(ncl_entity::FieldType type, bool is_delta)
    {
        switch (type)
        {
            case ncl_entity::FieldType::BYTE:
                return ncl_entity::FieldValue{static_cast<uint8_t>(READ_BYTE())};

            case ncl_entity::FieldType::USHORT:
            case ncl_entity::FieldType::STRING_ID:
                return ncl_entity::FieldValue{static_cast<uint16_t>(READ_SHORT())};

            case ncl_entity::FieldType::UINT:
                {
                    int val = READ_LONG();
                    return ncl_entity::FieldValue{*reinterpret_cast<uint32_t*>(&val)};
                }

            case ncl_entity::FieldType::FLOAT:
                return ncl_entity::FieldValue{READ_FLOAT()};

            case ncl_entity::FieldType::COORD:
                return ncl_entity::FieldValue{(READ_COORD())};

            case ncl_entity::FieldType::ARRAY_STRING_ID_PAIRS:
            case ncl_entity::FieldType::ARRAY_STRING_ID_PAIRS_DELTA:
                {
                    uint8_t added_count = READ_BYTE();
                    if (!READ_OK())
                    {
                        return ncl_entity::FieldValue{ncl_entity::ArrayStringIdPairsDelta{}};
                    }

                    if (is_delta && added_count == 0xFF)
                    {
                        return ncl_entity::FieldValue{ncl_entity::ArrayStringIdPairsDelta{.clear = true}};
                    }

                    ncl_entity::ArrayStringIdPairsDelta delta;
                    delta.added.reserve(added_count);
                    for (uint8_t i = 0; i < added_count; ++i)
                    {
                        uint16_t orig = static_cast<uint16_t>(READ_SHORT());
                        uint16_t repl = static_cast<uint16_t>(READ_SHORT());
                        delta.added.emplace_back(orig, repl);
                    }

                    uint8_t removed_count = READ_BYTE();
                    if (!READ_OK())
                    {
                        return ncl_entity::FieldValue{std::move(delta)};
                    }

                    delta.removed.reserve(removed_count);
                    for (uint8_t i = 0; i < removed_count; ++i)
                    {
                        uint16_t orig = static_cast<uint16_t>(READ_SHORT());
                        uint16_t repl = static_cast<uint16_t>(READ_SHORT());
                        delta.removed.emplace_back(orig, repl);
                    }

                    return ncl_entity::FieldValue{std::move(delta)};
                }
        }

        return ncl_entity::FieldValue{uint8_t(0)};
    }

    bool IsTypeValid(uint8_t type_id)
    {
        return type_id > 0 && type_id < std::size(g_Handlers) && g_Handlers[type_id] != nullptr;
    }

    int MsgFunc_EntitySyncCreate(const char* msg_name, int size, void* buf)
    {
        BEGIN_READ(buf, size);

        uint8_t type_id = READ_BYTE();
        if (!READ_OK())
        {
            return 1;
        }

        uint16_t entity_id = static_cast<uint16_t>(READ_SHORT());
        if (!READ_OK())
        {
            return 1;
        }

        if (!IsTypeValid(type_id))
        {
            return 1;
        }

        NclEntityFields fields;
        fields.reserve(g_Handlers[type_id]->Fields().size());
        for (const ClientNclEntityFieldDescriptor& field : g_Handlers[type_id]->Fields())
        {
            fields[field.field_bit] = ReadFieldValue(field.type, false);
        }

        if (!READ_OK())
        {
            return 1;
        }

        g_Handlers[type_id]->OnCreate(entity_id, fields);

        return 1;
    }

    int MsgFunc_EntitySyncUpdate(const char* msg_name, int size, void* buf)
    {
        BEGIN_READ(buf, size);

        uint8_t type_id = READ_BYTE();
        if (!READ_OK())
        {
            return 1;
        }

        uint16_t entity_id = static_cast<uint16_t>(READ_SHORT());
        if (!READ_OK())
        {
            return 1;
        }

        uint16_t field_mask = static_cast<uint16_t>(READ_SHORT());
        if (!READ_OK())
        {
            return 1;
        }

        if (!IsTypeValid(type_id))
        {
            return 1;
        }

        NclEntityFields fields;
        fields.reserve(g_Handlers[type_id]->Fields().size());
        for (const ClientNclEntityFieldDescriptor& field : g_Handlers[type_id]->Fields())
        {
            if (field_mask & field.field_bit)
            {
                fields[field.field_bit] = ReadFieldValue(field.type, true);
            }
        }

        if (!READ_OK())
        {
            return 1;
        }

        g_Handlers[type_id]->OnUpdate(entity_id, field_mask, fields);

        return 1;
    }

    int MsgFunc_EntitySyncDestroy(const char* msg_name, int size, void* buf)
    {
        BEGIN_READ(buf, size);

        uint8_t type_id = READ_BYTE();
        if (!READ_OK())
        {
            return 1;
        }

        uint16_t entity_id = static_cast<uint16_t>(READ_SHORT());
        if (!READ_OK())
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
