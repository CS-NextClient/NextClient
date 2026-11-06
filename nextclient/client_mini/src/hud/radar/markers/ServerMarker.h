#pragma once

class RadarDrawContext;

#include <ncl_math/vec3.h>
#include <ncl_math/color.h>

// The shape both re-purposed messages draw: a ring with a cross, in the marker color.
void ServerMarker_DrawShape(RadarDrawContext& draw, ncl_math::Vector2 screen, int size, ncl_math::Color color);

// A re-purposed message has no entity to interpolate against, so its marker eases toward the last
// message and snaps where the message jumps further than a marker should sweep. dt is in seconds.
ncl_math::Vector3 ServerMarker_Ease(ncl_math::Vector3 pos, ncl_math::Vector3 target, float dt);
