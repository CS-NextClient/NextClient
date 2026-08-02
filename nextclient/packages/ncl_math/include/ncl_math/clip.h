#pragma once

#include <span>

#include "vec3.h"

namespace ncl_math
{
    struct UvVertex
    {
        Vector2 pos;
        Vector2 uv;
    };

    // axis 0 = x, 1 = y; keep_greater keeps the >= side. out must hold in.size() + 1 vertices.
    inline int ClipHalfPlane(std::span<const UvVertex> in, int axis, float bound, bool keep_greater, std::span<UvVertex> out)
    {
        int n = (int)in.size();
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
