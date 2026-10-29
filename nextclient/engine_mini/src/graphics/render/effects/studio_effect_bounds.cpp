#include "graphics/render/effects/studio_effect_bounds.h"

#include <algorithm>
#include <cmath>

#include <ncl_math/aabb.h>

#include "graphics/render/model/StudioGeometry.h"
#include "graphics/render/StudioQueue.h"

render::effects::Rect_t render::effects::ProjectBounds(const StudioGpuModel& model, const StudioDrawPacket& packet, float radius)
{
    const Rect_t full = {0, 0, packet.viewport[2], packet.viewport[3]};
    if (model.bounds.empty())
    {
        return full;
    }
    ncl_math::Aabb2 bounds{{1.0f, 1.0f}, {-1.0f, -1.0f}};

    for (const StudioBoneBounds& box : model.bounds)
    {
        const float* bone = packet.bones.data() + box.bone * 12;
        for (int corner = 0; corner < 8; corner++)
        {
            float local[4] = {box.minimum[0], box.minimum[1], box.minimum[2], 1};
            for (int axis = 0; axis < 3; axis++)
            {
                if ((corner & (1 << axis)) != 0)
                {
                    local[axis] = box.maximum[axis];
                }
            }
            float world[4] = {0, 0, 0, 1};
            float eye[4]{};
            float clip[4]{};
            for (int row = 0; row < 3; row++)
            {
                for (int column = 0; column < 4; column++)
                {
                    world[row] += bone[row * 4 + column] * local[column];
                }
            }
            for (int row = 0; row < 4; row++)
            {
                for (int column = 0; column < 4; column++)
                {
                    eye[row] += packet.modelview[column * 4 + row] * world[column];
                }
            }
            for (int row = 0; row < 4; row++)
            {
                for (int column = 0; column < 4; column++)
                {
                    clip[row] += packet.projection[column * 4 + row] * eye[column];
                }
            }
            if (!std::isfinite(clip[3]) || clip[3] <= 0.00001f || clip[2] <= -clip[3])
            {
                return full;
            }
            ncl_math::Vector2 projected;

            for (int axis = 0; axis < 2; axis++)
            {
                const float value = std::clamp(clip[axis] / clip[3], -1.0f, 1.0f);
                if (!std::isfinite(value))
                {
                    return full;
                }
                projected[axis] = value;
            }

            bounds.Include(projected);
        }
    }
    const int x = std::clamp(static_cast<int>(std::floor((bounds.min.x + 1) * 0.5f * full[2] - radius)), 0, full[2]);
    const int y = std::clamp(static_cast<int>(std::floor((bounds.min.y + 1) * 0.5f * full[3] - radius)), 0, full[3]);
    const int right = std::clamp(static_cast<int>(std::ceil((bounds.max.x + 1) * 0.5f * full[2] + radius)), 0, full[2]);
    const int top = std::clamp(static_cast<int>(std::ceil((bounds.max.y + 1) * 0.5f * full[3] + radius)), 0, full[3]);
    return {x, y, right - x, top - y};
}
