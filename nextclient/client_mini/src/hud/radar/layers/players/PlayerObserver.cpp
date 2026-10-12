#include "PlayerObserver.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kIconSizeScale = 0.0185f;
    constexpr float kIconMinSize = 16.0f;
    constexpr float kIconMaxSize = 28.0f;
    constexpr float kIconPadding = 4.0f;
} // namespace

PlayerObserverMarker PlayerObserver_ChooseMarker(
    const FrameContext& context,
    const goldsrc::cl_entity_t* entity,
    bool alive,
    ncl_math::Vector3 camera,
    float camera_yaw
)
{
    if (!context.free_camera && alive && entity && entity->player && RadarFrame_IsEntityCurrent(context, entity))
    {
        return {RadarFrame_GetPlayerPosition(*entity), entity->angles[1], entity->index};
    }

    return {camera, camera_yaw, 0};
}

PlayerObserverIcon PlayerObserver_BuildStatusIcon(const RadarBox& box, const RadarShape& shape)
{
    float half_extent = std::clamp(box.screen_h * kIconSizeScale, kIconMinSize, kIconMaxSize) * 0.5f;
    float radius = half_extent * 1.45f;
    int inset = (int)std::ceil(radius + kIconPadding);
    ncl_math::Vector2 center = shape.Clamp(inset, shape.center + ncl_math::Vector2{shape.radius, shape.radius});

    PlayerObserverIcon icon{};
    icon.center = center;
    icon.radius = radius;
    icon.half_extent = half_extent;
    icon.line_width = std::max(2.1f, half_extent * 0.19f);

    return icon;
}
