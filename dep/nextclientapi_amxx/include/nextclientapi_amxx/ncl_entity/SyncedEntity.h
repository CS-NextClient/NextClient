#pragma once
#include <unordered_map>

#include "VisibilityReference.h"
#include "FieldValue.h"
#include "../type_resolver.h"

namespace ncl_entity
{
    struct SyncedEntity
    {
        uint8_t type_id{};
        uint16_t entity_id{};
        edict_t* bound_edict{};
        VisibilityReference visibility_ref{};
        std::unordered_map<uint16_t, FieldValue> field_cache{};
        int last_serialnumber{};
        std::unordered_map<uint16_t, FieldValue> pending_deltas{};
    };
} // namespace ncl_entity
