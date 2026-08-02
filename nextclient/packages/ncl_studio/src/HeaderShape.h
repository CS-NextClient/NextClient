#pragma once

#include <string>

// studio.h uses vec3_t without declaring it.
#include <tier0/basetypes.h>
#include <studio.h>

#include "ncl_studio/StudioMesh.h"

namespace ncl_studio
{
    // The header-shape rules both the validator and the mesh builder accept a file by,
    // in one home so the two cannot drift. Empty means accepted. Zero bones is what a
    // texture-only companion declares; geometry without bones is the case that cannot
    // be built, and the bone count has to be within what the format allows before it
    // can bound any per-vertex bone byte.
    inline std::string HeaderShapeError(const studiohdr_t* header)
    {
        if (header->version != kStudioVersion)
        {
            return "unsupported studio version " + std::to_string(header->version) +
                   ", expected " + std::to_string(kStudioVersion);
        }

        if (header->numbones < 0 || header->numbones > MAXSTUDIOBONES)
        {
            return "model declares " + std::to_string(header->numbones) + " bones, the format allows 0.." +
                   std::to_string(MAXSTUDIOBONES);
        }

        if (header->numbones == 0 && header->numbodyparts != 0)
        {
            return "model declares " + std::to_string(header->numbodyparts) + " body parts and no bones to move them";
        }

        return {};
    }
}
