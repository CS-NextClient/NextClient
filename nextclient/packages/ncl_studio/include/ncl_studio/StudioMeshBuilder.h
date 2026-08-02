#pragma once

#include <cstddef>

#include <ncl_utils/safe_result.h>

#include "StudioMesh.h"

namespace ncl_studio
{
    // Converts a whole .mdl buffer into indexed triangles. Models arrive over the
    // network, so every offset and index the file declares is checked against the
    // buffer instead of being trusted.
    ncl_utils::ResultT<StudioMesh> BuildStudioMesh(const void* data, size_t size);
}
