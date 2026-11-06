#include "ServerMarker.h"

#include "hud/radar/render/RadarDraw.h"

void ServerMarker_DrawShape(RadarDrawContext& draw, ncl_math::Vector2 screen, int size, ncl_math::Color color)
{
    draw.DrawRing(screen, (float)size + 1.5f, (float)size - 0.5f, color);
    draw.DrawCross(screen, size * 0.75f, 1.5f, color);
}
