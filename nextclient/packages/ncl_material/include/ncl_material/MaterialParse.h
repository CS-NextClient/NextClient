#pragma once

#include <string>
#include <string_view>

#include <ncl_utils/safe_result.h>

#include "MaterialDoc.h"

namespace ncl_material
{
    ncl_utils::ResultT<MaterialDoc> ParseMaterialDoc(std::string_view json);

    // The inverse, so that whatever writes a document writes the same shape the client
    // reads. A hand-written serialiser somewhere else is how a format grows a second
    // dialect that only one of the two ends understands.
    std::string WriteMaterialDoc(const MaterialDoc& doc);
}
