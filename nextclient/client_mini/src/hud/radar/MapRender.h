#pragma once

#include <ncl_math/aabb.h>

typedef struct model_s model_t;

// A top-down image of the BSP world rendered once at level load, used as the radar
// map on maps that ship no overviews/<map>.txt. Floors and walls are drawn from
// above (ceilings skipped) shaded by height.
struct RuntimeMap
{
    unsigned int texture; // GL texture id (0 when invalid)
    ncl_math::Aabb2 bounds; // world XY covered by the texture
    bool valid;
};

// textured: draw with the map's own surface textures; otherwise flat height shading.
// slice: keep only floors near the dominant play level, so upper structures stop
// hiding interior passages (one level only).
RuntimeMap MapRender_Generate(model_t* world, bool textured, bool slice);
void MapRender_Free(RuntimeMap& map);
