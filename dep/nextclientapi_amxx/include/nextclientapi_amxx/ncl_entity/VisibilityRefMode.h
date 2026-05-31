#pragma once

namespace ncl_entity
{
    enum class VisibilityRefMode
    {
        BROADCAST,
        EDICT_PVS,
        EDICT_PAS,
        POSITION_PVS,
        POSITION_PAS
    };
}
