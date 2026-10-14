#pragma once

#include <ncl_math/color.h>

namespace RadarPalette
{
    inline constexpr ncl_math::Color kTerrorist{255, 60, 35};
    inline constexpr ncl_math::Color kCounterTerrorist{60, 180, 255};
    inline constexpr ncl_math::Color kHighlight{50, 220, 70};
    inline constexpr ncl_math::Color kGold{255, 215, 0};
    // A bomb message the server re-purposed to mark something else: deliberately not
    // the bomb's gold, so it does not read as a C4 that is not there.
    inline constexpr ncl_math::Color kServerMarker{255, 96, 64};
} // namespace RadarPalette
