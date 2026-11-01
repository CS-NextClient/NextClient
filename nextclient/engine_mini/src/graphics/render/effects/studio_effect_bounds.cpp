#include "graphics/render/effects/studio_effect_bounds.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <xmmintrin.h>

#include <ncl_math/aabb.h>
#include <ncl_perf/profiler.h>

#include "graphics/render/model/StudioGeometry.h"
#include "graphics/render/StudioQueue.h"

namespace
{
    __m128 TransformRow(const float* matrix, int row, int stride, const __m128* source)
    {
        __m128 result = _mm_setzero_ps();
        for (int column = 0; column < 4; ++column)
        {
            const int offset = stride == 1 ? row * 4 + column : column * 4 + row;
            result = _mm_add_ps(result, _mm_mul_ps(_mm_set1_ps(matrix[offset]), source[column]));
        }

        return result;
    }

    bool IsFinite(__m128 value)
    {
        const __m128 magnitude = _mm_andnot_ps(_mm_set1_ps(-0.0f), value);
        return _mm_movemask_ps(_mm_cmple_ps(magnitude, _mm_set1_ps((std::numeric_limits<float>::max)()))) == 15;
    }

    __m128 Clamp(__m128 value)
    {
        // The second operand retains NaN and signed zero, as std::clamp does.
        value = _mm_max_ps(_mm_set1_ps(-1.0f), value);
        return _mm_min_ps(_mm_set1_ps(1.0f), value);
    }
} // namespace

render::effects::Rect_t render::effects::ProjectBounds(const StudioGpuModel& model, const StudioDrawPacket& packet, float radius)
{
    NCL_PROFILE_NAMED_SCOPE("ProjectBounds");

    const Rect_t full = {0, 0, packet.viewport[2], packet.viewport[3]};
    if (model.bounds.empty())
    {
        return full;
    }

    ncl_math::Aabb2 bounds{{1.0f, 1.0f}, {-1.0f, -1.0f}};

    for (const StudioBoneBounds& box : model.bounds)
    {
        const float* bone = packet.bones.data() + box.bone * 12;

        for (int corner = 0; corner < 8; corner += 4)
        {
            const __m128 local[4] = {
                _mm_setr_ps(box.minimum.x, box.maximum.x, box.minimum.x, box.maximum.x),
                _mm_setr_ps(box.minimum.y, box.minimum.y, box.maximum.y, box.maximum.y),
                _mm_set1_ps(corner == 0 ? box.minimum.z : box.maximum.z),
                _mm_set1_ps(1.0f)
            };
            __m128 world[4] = {_mm_setzero_ps(), _mm_setzero_ps(), _mm_setzero_ps(), _mm_set1_ps(1.0f)};
            __m128 eye[4];
            __m128 clip[4];

            for (int row = 0; row < 3; ++row)
            {
                world[row] = TransformRow(bone, row, 1, local);
            }

            for (int row = 0; row < 4; ++row)
            {
                eye[row] = TransformRow(packet.modelview.data(), row, 4, world);
            }

            for (int row = 0; row < 4; ++row)
            {
                clip[row] = TransformRow(packet.projection.data(), row, 4, eye);
            }

            const __m128 small_w = _mm_cmple_ps(clip[3], _mm_set1_ps(0.00001f));
            const __m128 behind_near = _mm_cmple_ps(clip[2], _mm_xor_ps(clip[3], _mm_set1_ps(-0.0f)));
            if (!IsFinite(clip[3]) || _mm_movemask_ps(_mm_or_ps(small_w, behind_near)) != 0)
            {
                return full;
            }

            const __m128 x = Clamp(_mm_div_ps(clip[0], clip[3]));
            const __m128 y = Clamp(_mm_div_ps(clip[1], clip[3]));
            if (!IsFinite(x) || !IsFinite(y))
            {
                return full;
            }

            float projected_x[4];
            float projected_y[4];
            _mm_storeu_ps(projected_x, x);
            _mm_storeu_ps(projected_y, y);

            for (int lane = 0; lane < 4; ++lane)
            {
                bounds.Include({projected_x[lane], projected_y[lane]});
            }
        }
    }

    const int x = std::clamp(static_cast<int>(std::floor((bounds.min.x + 1) * 0.5f * full[2] - radius)), 0, full[2]);
    const int y = std::clamp(static_cast<int>(std::floor((bounds.min.y + 1) * 0.5f * full[3] - radius)), 0, full[3]);
    const int right = std::clamp(static_cast<int>(std::ceil((bounds.max.x + 1) * 0.5f * full[2] + radius)), 0, full[2]);
    const int top = std::clamp(static_cast<int>(std::ceil((bounds.max.y + 1) * 0.5f * full[3] + radius)), 0, full[3]);

    return {x, y, right - x, top - y};
}
