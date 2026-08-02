#pragma once

#include "vec3.h"

// Convex-polygon clipping (the Sutherland-Hodgman half-plane step) with linear
// interpolation of per-vertex attributes across each cut edge.
namespace ncl_math
{
    // A convex-polygon vertex carrying a position and an interpolated texture
    // coordinate.
    struct UvVertex
    {
        Vector2 pos;
        Vector2 uv;
    };

    // Clips a convex polygon against one axis-aligned half-plane. axis 0 = x,
    // 1 = y; keep_greater selects the >= side. Writes at most n + 1 vertices to
    // out; returns the count.
    inline int ClipHalfPlane(const UvVertex* in, int n, int axis, float bound, bool keep_greater, UvVertex* out)
    {
        int cnt = 0;

        for (int i = 0; i < n; i++)
        {
            const UvVertex& a = in[i];
            const UvVertex& b = in[(i + 1) % n];
            float da = a.pos[axis] - bound;
            float db = b.pos[axis] - bound;
            bool ina = keep_greater ? da >= 0.0f : da <= 0.0f;
            bool inb = keep_greater ? db >= 0.0f : db <= 0.0f;

            if (ina)
            {
                out[cnt++] = a;
            }

            if (ina != inb)
            {
                float f = da / (da - db);
                out[cnt++] = {Lerp(a.pos, b.pos, f), Lerp(a.uv, b.uv, f)};
            }
        }

        return cnt;
    }
} // namespace ncl_math
