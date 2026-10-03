#pragma once

#include <ncl_math/vec3.h>

// The shape both re-purposed messages draw: a ring with a cross, in the marker colour.
void ServerMarker_DrawShape(ncl_math::Vector2 screen, int size);

// A re-purposed message has no entity to interpolate against, so its marker eases toward the last
// message and snaps where the message jumps further than a marker should sweep. dt is in seconds.
ncl_math::Vector2 ServerMarker_Ease(ncl_math::Vector2 pos, ncl_math::Vector2 target, float dt);
