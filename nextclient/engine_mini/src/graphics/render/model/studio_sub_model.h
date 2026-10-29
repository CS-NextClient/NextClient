#pragma once

#include <goldsrc/format/studio.h>

namespace render
{
    // raw must be readable engine output from StudioSetupModel, either a submodel or its pointer.
    const goldsrc::mstudiomodel_t* Studio_ResolveSubModel(const void* raw, const goldsrc::studiohdr_t* header);
} // namespace render
