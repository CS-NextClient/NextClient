#pragma once

#include <algorithm>
#include <array>
#include <span>

#include <ncl_math/vec3.h>

namespace
{
    // Clamped points can produce sub-pixel segments whose direction is noise.
    constexpr float kMinSegmentLen = ncl_math::kNearZero;

    ncl_math::Vector2 RadarPolyline_ComputeMiterOffset(ncl_math::Vector2 first, ncl_math::Vector2 second, float half_width)
    {
        ncl_math::Vector2 sum = first + second;
        float length = sum.Length();
        if (length < ncl_math::kNearZero)
        {
            return second.Perp() * half_width;
        }

        ncl_math::Vector2 miter = (sum / length).Perp();
        float align = std::max(miter.Dot(second.Perp()), 0.5f);
        return miter * (half_width / align);
    }
} // namespace

template <class EmitQuad>
void RadarPolyline_ForEachQuad(std::span<const ncl_math::Vector2> points, float width, EmitQuad&& emit)
{
    float half_width = width * 0.5f;
    ncl_math::Vector2 prepared_direction{};
    ncl_math::Vector2 prepared_offset{};
    bool prepared = false;

    for (size_t i = 0; i + 1 < points.size(); i++)
    {
        ncl_math::Vector2 direction = prepared_direction;

        if (!prepared)
        {
            direction = points[i + 1] - points[i];
            float length = direction.Length();
            if (length < kMinSegmentLen)
            {
                prepared = false;
                continue;
            }

            direction = direction / length;
        }

        ncl_math::Vector2 next_direction{};
        bool has_next = false;

        if (i + 2 < points.size())
        {
            next_direction = points[i + 2] - points[i + 1];
            float next_length = next_direction.Length();
            if (next_length >= kMinSegmentLen)
            {
                next_direction = next_direction / next_length;
                has_next = true;
            }
        }

        ncl_math::Vector2 start_offset = prepared ? prepared_offset : direction.Perp() * half_width;
        ncl_math::Vector2 end_offset =
            has_next ? RadarPolyline_ComputeMiterOffset(direction, next_direction, half_width) : direction.Perp() * half_width;
        emit(
            i,
            std::array<ncl_math::Vector2, 4>{
                points[i] + start_offset, points[i] - start_offset, points[i + 1] - end_offset, points[i + 1] + end_offset
            }
        );

        prepared = has_next;
        prepared_direction = next_direction;
        prepared_offset = end_offset;
    }
}
