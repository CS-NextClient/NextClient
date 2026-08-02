#include "ServerMarker.h"

#include "RadarDraw.h"
#include "RadarPalette.h"

void ServerMarker_DrawShape(ncl_math::Vector2 screen, int size)
{
    ncl_math::Color col = RadarPalette::kServerMarker;
    RadarDraw_DrawRing(screen, (float)size + 1.5f, (float)size - 0.5f, col);
    RadarDraw_DrawCross(screen, size * 0.75f, 1.5f, col);
}
