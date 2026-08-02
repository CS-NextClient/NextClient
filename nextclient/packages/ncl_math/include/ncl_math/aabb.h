#pragma once

#include "vec3.h"

namespace ncl_math
{
    // Axis-aligned 2D bounding box. Start from Empty() and grow with Include(), or
    // build from a known min/max pair.
    struct Aabb2
    {
        Vector2 min, max;

        Vector2 Center() const { return (min + max) * 0.5f; }
        Vector2 Size() const { return max - min; }
        bool Contains(Vector2 p) const { return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y; }
        Aabb2 Expanded(float m) const { return {{min.x - m, min.y - m}, {max.x + m, max.y + m}}; }

        void Include(Vector2 p)
        {
            min = Min(min, p);
            max = Max(max, p);
        }

        static Aabb2 Empty()
        {
            return {{1.0e30f, 1.0e30f}, {-1.0e30f, -1.0e30f}};
        }
    };
} // namespace ncl_math
