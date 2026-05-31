#pragma once
#include <cstdint>
#include <functional>

#include "FieldValue.h"
#include "FieldType.h"
#include "FieldTracking.h"
#include "../type_resolver.h"

namespace ncl_entity
{
    struct EntityFieldDescriptor
    {
        uint16_t field_bit{};
        FieldType type = FieldType::BYTE;
        FieldTracking tracking = FieldTracking::MANUAL;
        std::function<FieldValue(edict_t* bound)> getter{};

        EntityFieldDescriptor() = default;
        EntityFieldDescriptor(uint16_t bit, FieldType t, FieldTracking tr, std::function<FieldValue(edict_t*)> g) :
            field_bit(bit),
            type(t),
            tracking(tr),
            getter(std::move(g))
        {}
    };
} // namespace ncl_entity
