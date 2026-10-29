#pragma once

namespace render
{
    inline constexpr unsigned int kStudioBonesBinding = 0;
    inline constexpr int kStudioBaseTextureUnit = 0;
    // Units 1 and 2 carry the engine's lightmap and detail bookkeeping.
    inline constexpr int kFirstMaterialUnit = 3;
    inline constexpr int kStudioMaterialUnits = 8;
    inline constexpr int kStudioTouchedTextureUnits = kFirstMaterialUnit + kStudioMaterialUnits;
} // namespace render
