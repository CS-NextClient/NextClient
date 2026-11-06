#pragma once

#include <string>
#include <string_view>

#include <ncl_utils/safe_result.h>

#include "ncl_material/MaterialDoc.h"

namespace ncl_material
{
    ncl_utils::ResultT<MaterialDoc> ParseMaterialDoc(std::string_view json);

    std::string WriteMaterialDoc(const MaterialDoc& doc);
} // namespace ncl_material
