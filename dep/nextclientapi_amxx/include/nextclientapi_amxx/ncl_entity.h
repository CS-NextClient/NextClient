#pragma once
#include <cstdint>
#include "ncl_entity/EntityTypeDescriptor.h"
#include "ncl_entity/VisibilityReference.h"
#include "ncl_entity/FieldValue.h"
#include "ncl_entity/SyncedEntity.h"
#include "type_resolver.h"

namespace ncl_entity
{
    enum class EntityTypeId : uint8_t
    {
        Unknown,
        Player,
        Weapon,

        // What a server has dressed an entity in. Any edict can wear one, which is why it
        // is a type of its own rather than a field of the weapon.
        Material
    };
}
