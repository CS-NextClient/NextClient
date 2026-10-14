#include "RadarScoreboard.h"

#include <cmath>

namespace
{
    constexpr float kGapHeightFraction = 0.01f;
}

ncl_math::Vector2 RadarScoreboard::Update(
    std::uint32_t panel,
    const ncl_math::Aabb2& bounds,
    ncl_math::Vector2 screen_size,
    const ncl_math::Aabb2* radar
)
{
    ncl_math::Vector2 pos = bounds.min;
    ncl_math::Vector2 size = bounds.Size();
    // A stock relayout or a recreated panel owns its new position.
    if (shifted_ && panel == panel_ && pos == placed_ && size == size_ && screen_size == screen_size_)
    {
        pos = original_;
    }
    Reset();

    if (!radar || !radar->Overlaps({pos, pos + size}))
    {
        return pos;
    }

    float gap = screen_size.y * kGapHeightFraction;
    ncl_math::Vector2 candidates[] = {
        {std::ceil(radar->max.x + gap), pos.y},
        {pos.x, std::ceil(radar->max.y + gap)},
    };
    for (ncl_math::Vector2 candidate : candidates)
    {
        if (candidate.x < 0.0f || candidate.y < 0.0f || candidate.x + size.x > screen_size.x || candidate.y + size.y > screen_size.y)
        {
            continue;
        }
        shifted_ = true;
        panel_ = panel;
        original_ = pos;
        placed_ = candidate;
        size_ = size;
        screen_size_ = screen_size;
        return candidate;
    }
    return pos;
}

void RadarScoreboard::Reset()
{
    shifted_ = false;
}
