#pragma once

namespace render
{
    inline constexpr unsigned int kStudioInstanceMatrixSlot = 5;
    inline constexpr unsigned int kStudioInstanceMatrixRows = 3;
    inline constexpr unsigned int kStudioInstanceRangeSlot = kStudioInstanceMatrixSlot + kStudioInstanceMatrixRows;
    inline constexpr unsigned int kStudioBonesBinding = 0;
    inline constexpr int kStudioBaseTextureUnit = 0;
    // Units 1 and 2 carry the engine's lightmap and detail bookkeeping.
    inline constexpr int kFirstMaterialUnit = 3;
    inline constexpr int kStudioMaterialUnits = 8;
    inline constexpr int kStudioTouchedTextureUnits = kFirstMaterialUnit + kStudioMaterialUnits;
} // namespace render
