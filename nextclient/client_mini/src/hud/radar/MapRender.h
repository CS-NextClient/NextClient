#pragma once

#include <ncl_math/aabb.h>

typedef struct model_s model_t;

struct RuntimeMap
{
    unsigned int texture;
    ncl_math::Aabb2 bounds; // world XY covered by the texture
    bool valid;
};

RuntimeMap MapRender_Generate(model_t* world, bool textured, bool slice);
void MapRender_Free(RuntimeMap& map);
