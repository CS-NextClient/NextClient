#pragma once
#include <vector>
#include <functional>

#include "VisibilityReference.h"
#include "EntityFieldDescriptor.h"
#include "../type_resolver.h"

namespace ncl_entity
{
    struct EntityTypeDescriptor
    {
        uint8_t type_id{};
        std::vector<EntityFieldDescriptor> fields{};
        std::function<VisibilityReference(edict_t* bound)> visibility_getter{};
    };
} // namespace ncl_entity
