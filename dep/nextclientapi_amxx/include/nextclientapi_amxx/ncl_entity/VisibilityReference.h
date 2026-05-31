#pragma once
#include "VisibilityRefMode.h"
#include "../type_resolver.h"

namespace ncl_entity
{
    struct VisibilityReference
    {
        VisibilityRefMode mode = VisibilityRefMode::BROADCAST;
        edict_t* edict{};
        float position[3]{};

        static VisibilityReference Broadcast()
        {
            return {};
        }

        static VisibilityReference BindEdictPVS(edict_t* edict)
        {
            VisibilityReference a;
            a.mode = VisibilityRefMode::EDICT_PVS;
            a.edict = edict;
            return a;
        }

        static VisibilityReference BindEdictPAS(edict_t* edict)
        {
            VisibilityReference a;
            a.mode = VisibilityRefMode::EDICT_PAS;
            a.edict = edict;
            return a;
        }

        static VisibilityReference PositionPVS(const float* pos)
        {
            VisibilityReference a;
            a.mode = VisibilityRefMode::POSITION_PVS;
            std::memcpy(a.position, pos, sizeof(float) * 3);
            return a;
        }

        static VisibilityReference PositionPAS(const float* pos)
        {
            VisibilityReference a;
            a.mode = VisibilityRefMode::POSITION_PAS;
            std::memcpy(a.position, pos, sizeof(float) * 3);
            return a;
        }
    };
} // namespace ncl_entity
