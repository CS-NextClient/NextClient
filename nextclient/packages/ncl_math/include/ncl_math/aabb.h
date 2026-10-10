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
        bool Overlaps(const Aabb2& o) const { return max.x >= o.min.x && min.x <= o.max.x && max.y >= o.min.y && min.y <= o.max.y; }

        // Requires ordered bounds on both axes.
        Vector2 ClosestPoint(Vector2 point) const
        {
            return {Clamp(point.x, min.x, max.x), Clamp(point.y, min.y, max.y)};
        }

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

    struct Aabb3
    {
        Vector3 min{}, max{};

        Vector3 Center() const
        {
            return (min + max) * 0.5f;
        }

        Vector3 Size() const
        {
            return max - min;
        }

        void Include(Vector3 point)
        {
            for (int axis = 0; axis < 3; axis++)
            {
                min[axis] = std::min(min[axis], point[axis]);
                max[axis] = std::max(max[axis], point[axis]);
            }
        }
        void Include(const Aabb3& bounds)
        {
            for (int axis = 0; axis < 3; axis++)
            {
                min[axis] = std::min(min[axis], bounds.min[axis]);
                max[axis] = std::max(max[axis], bounds.max[axis]);
            }
        }
        Vector3 ClosestPoint(Vector3 point) const
        {
            return {Clamp(point.x, min.x, max.x), Clamp(point.y, min.y, max.y), Clamp(point.z, min.z, max.z)};
        }

        bool Contains(Vector3 point) const
        {
            return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y && point.z >= min.z && point.z <= max.z;
        }

        bool ContainsHalfOpen(Vector3 point) const
        {
            return point.x >= min.x && point.x < max.x && point.y >= min.y && point.y < max.y && point.z >= min.z && point.z < max.z;
        }

        bool Overlaps(const Aabb3& bounds) const
        {
            return max.x >= bounds.min.x && min.x <= bounds.max.x && max.y >= bounds.min.y && min.y <= bounds.max.y &&
                   max.z >= bounds.min.z && min.z <= bounds.max.z;
        }

        bool OverlapsInterior(const Aabb3& bounds) const
        {
            return std::max(min.x, bounds.min.x) < std::min(max.x, bounds.max.x) &&
                   std::max(min.y, bounds.min.y) < std::min(max.y, bounds.max.y) &&
                   std::max(min.z, bounds.min.z) < std::min(max.z, bounds.max.z);
        }

        static Aabb3 Empty()
        {
            return {{1.0e30f, 1.0e30f, 1.0e30f}, {-1.0e30f, -1.0e30f, -1.0e30f}};
        }
    };
} // namespace ncl_math
