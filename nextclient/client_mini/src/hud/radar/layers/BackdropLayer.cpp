#include "BackdropLayer.h"

#include <algorithm>
#include <span>

#include "main.h"

#include <goldsrc/client_api.h>
#include <goldsrc/cvar.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/entity.h>
#include <goldsrc/triangle_api.h>
#include <ncl_math/aabb.h>
#include <ncl_math/clip.h>
#include <ncl_math/color.h>
#include <next_client_mini/radar_cvars.h>

#include "hud/radar/RadarConfig.h"
#include "hud/radar/RadarDraw.h"
#include "hud/radar/SmokeRegistry.h"

namespace
{
    constexpr RadarCvarDef kOpacityCvar = RadarCvarDef_Get("hud_radar_opacity");
    static_assert(kOpacityCvar.name);

    // Matches SmokeRegistry's own table size: the registry never reports more.
    constexpr int kMaxSmokeDiscs = 16;

    float ComputeOpacityFraction()
    {
        return std::clamp(g_RadarCfg.opacity->value, kOpacityCvar.min_value, kOpacityCvar.max_value) / kOpacityCvar.max_value;
    }

    // out holds up to 8 vertices.
    int ClipTileToBox(std::span<const ncl_math::UvVertex, 4> quad, const RadarBox& box, std::span<ncl_math::UvVertex> out)
    {
        ncl_math::UvVertex a[10], b[10];
        int n = ncl_math::ClipHalfPlane(quad, 0, box.x, true, a);
        n = ncl_math::ClipHalfPlane({a, (size_t)n}, 0, box.x + box.size, false, b);
        n = ncl_math::ClipHalfPlane({b, (size_t)n}, 1, box.y, true, a);
        n = ncl_math::ClipHalfPlane({a, (size_t)n}, 1, box.y + box.size, false, out);
        return n;
    }
} // namespace

BackdropLayer::BackdropLayer(SmokeRegistry& smoke) :
    smoke_(smoke)
{
}

bool BackdropLayer::EnsureMap()
{
    overview_.EnsureLoaded(RadarMapMode_FromCvar(g_RadarCfg.map_mode->value));
    return overview_.is_valid();
}

void BackdropLayer::OnVidInit()
{
    overview_.Invalidate();
    clamped_sprite_ = nullptr; // sprite textures come back with default wrap
}

void BackdropLayer::Draw(const RadarFrame& frame)
{
    DrawBackground(frame);

    // Commit the queued background disc before the map: the no-shader tile path draws through
    // the engine TriAPI, which never triggers the batch flush, so the disc would land on top.
    RadarDraw_FlushPrims();

    if (overview_.is_runtime())
    {
        DrawRuntimeMap(frame);
    }
    else
    {
        DrawOverviewTiles(frame);
    }

    DrawSmoke(frame);
    DrawBorder(frame);
}

void BackdropLayer::DrawBackground(const RadarFrame& frame)
{
    float opacity = ComputeOpacityFraction();
    int a = (int)(opacity * 255.0f);

    if (a <= 0)
    {
        return;
    }

    const RadarBox& box = frame.box;
    bool round = frame.shape.round;

    if (g_RadarCfg.bg->value != 0.0f &&
        RadarDraw_DrawBlurBackground(box.x, box.y, box.size, !round, 0.35f, opacity, box.screen_w, box.screen_h))
    {
        return;
    }

    if (round)
    {
        RadarDraw_DrawDisc(frame.shape.center, frame.shape.radius, {0, 0, 0, a});
    }
    else
    {
        ncl_math::Color c = RadarDraw_ApplyFade({0, 0, 0});
        gEngfuncs.pfnFillRGBABlend((int)box.x, (int)box.y, (int)box.size, (int)box.size, c.r, c.g, c.b, a);
    }
}

void BackdropLayer::DrawRuntimeMap(const RadarFrame& frame)
{
    const RuntimeMap& runtime_map = overview_.get_runtime_map();
    ncl_math::Vector2 c0 = frame.t.WorldToScreen({runtime_map.bounds.min.x, runtime_map.bounds.min.y});
    ncl_math::Vector2 c1 = frame.t.WorldToScreen({runtime_map.bounds.max.x, runtime_map.bounds.min.y});
    ncl_math::Vector2 c2 = frame.t.WorldToScreen({runtime_map.bounds.max.x, runtime_map.bounds.max.y});
    ncl_math::Vector2 c3 = frame.t.WorldToScreen({runtime_map.bounds.min.x, runtime_map.bounds.max.y});

    float xy[8] = {c0.x, c0.y, c1.x, c1.y, c2.x, c2.y, c3.x, c3.y};
    const float uv[8] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};

    float opacity = ComputeOpacityFraction();

    RadarDraw_DrawMapTexture(
        runtime_map.texture,
        xy,
        uv, 
    {1.0f, 1.0f, 1.0f, opacity},
        frame.shape.center,
        frame.shape.radius,
        !frame.shape.round
    );
}

void BackdropLayer::EnsureSpriteClamped(goldsrc::model_t* map, int total_frames)
{
    if (map == clamped_sprite_)
    {
        return;
    }

    // GL_LINEAR samples past the tile edges and leaves transparent seams between tiles.
    goldsrc::triangleapi_t* tri = gEngfuncs.pTriAPI;

    for (int i = 0; i < total_frames; i++)
    {
        tri->SpriteTexture(map, i);
        RadarDraw_ClampBoundTexture();
    }

    clamped_sprite_ = map;
}

void BackdropLayer::DrawOverviewTile(const RadarBox& box, const TileCorners& corners, bool masked)
{
    goldsrc::triangleapi_t* tri = gEngfuncs.pTriAPI;

    if (masked)
    {
        // The mask shader discards outside the box; clipping here would cut its feathered rim.
        tri->Begin(goldsrc::TRI_QUADS);
        tri->TexCoord2f(0, 0);
        tri->Vertex3f(corners.base.x, corners.base.y, 0.0f);
        tri->TexCoord2f(0, 1);
        tri->Vertex3f(corners.right.x, corners.right.y, 0.0f);
        tri->TexCoord2f(1, 1);
        tri->Vertex3f(corners.far_corner.x, corners.far_corner.y, 0.0f);
        tri->TexCoord2f(1, 0);
        tri->Vertex3f(corners.up.x, corners.up.y, 0.0f);
        tri->End();
        return;
    }

    const ncl_math::UvVertex quad[4] = {
        {corners.base, {0.0f, 0.0f}},
        {corners.right, {0.0f, 1.0f}},
        {corners.far_corner, {1.0f, 1.0f}},
        {corners.up, {1.0f, 0.0f}},
    };
    ncl_math::UvVertex clipped[10];
    int cn = ClipTileToBox(quad, box, clipped);

    if (cn < 3)
    {
        return;
    }

    tri->Begin(goldsrc::TRI_TRIANGLE_FAN);

    for (int i = 0; i < cn; i++)
    {
        tri->TexCoord2f(clipped[i].uv.x, clipped[i].uv.y);
        tri->Vertex3f(clipped[i].pos.x, clipped[i].pos.y, 0.0f);
    }

    tri->End();
}

void BackdropLayer::DrawOverviewTiles(const RadarFrame& frame)
{
    goldsrc::model_t* map = overview_.get_sprite();

    if (!map)
    {
        return;
    }

    const RadarBox& box = frame.box;
    const RadarTransform& transform = frame.t;
    float opacity = ComputeOpacityFraction();

    goldsrc::triangleapi_t* tri = gEngfuncs.pTriAPI;
    tri->RenderMode(goldsrc::kRenderTransAlpha);
    tri->CullFace(goldsrc::TRI_NONE);
    tri->Color4f(1.0f, 1.0f, 1.0f, opacity);

    EnsureSpriteClamped(map, transform.total_frames);

    bool masked = g_RadarCfg.map_mask->value != 0.0f &&
        RadarDraw_BeginMask(frame.shape.center, frame.shape.radius, 1.5f, 0.0f, !frame.shape.round);

    ncl_math::Aabb2 box_rect = {{box.x, box.y}, {box.x + box.size, box.y + box.size}};
    ncl_math::Vector2 row = transform.tile_origin;
    int sprite_frame = 0;

    for (int ty = 0; ty < transform.y_tiles; ty++)
    {
        ncl_math::Vector2 base = row;

        for (int tx = 0; tx < transform.x_tiles; tx++)
        {
            if (sprite_frame >= transform.total_frames)
            {
                break;
            }

            TileCorners corners;
            corners.base = base;
            corners.right = base + transform.right;
            corners.up = base + transform.up;
            corners.far_corner = base + transform.right + transform.up;

            ncl_math::Aabb2 tile = ncl_math::Aabb2::Empty();
            tile.Include(corners.base);
            tile.Include(corners.right);
            tile.Include(corners.up);
            tile.Include(corners.far_corner);

            if (tile.Overlaps(box_rect))
            {
                tri->SpriteTexture(map, sprite_frame);
                DrawOverviewTile(box, corners, masked);
            }

            sprite_frame++;
            base = base + transform.up;
        }

        row = row + transform.right;
    }

    if (masked)
    {
        RadarDraw_EndMask();
    }

    tri->RenderMode(goldsrc::kRenderNormal);
}

void BackdropLayer::DrawSmoke(const RadarFrame& frame)
{
    SmokeRegistry::Disc smokes[kMaxSmokeDiscs];
    int n = smoke_.Collect(smokes, frame.time);

    if (n == 0)
    {
        return;
    }

    RadarDraw_SetPrimClip(frame.shape.center, frame.shape.radius, !frame.shape.round);
    RadarDraw_BeginPrims();

    for (int i = 0; i < n; i++)
    {
        ncl_math::Vector2 s = frame.t.WorldToScreen({smokes[i].center.x, smokes[i].center.y});
        ncl_math::Vector2 e = frame.t.WorldToScreen({smokes[i].center.x + smokes[i].radius, smokes[i].center.y});
        float sr = std::max(2.0f, (e - s).Length());
        RadarDraw_DrawDisc(s, sr, {210, 210, 210, 110});
    }

    RadarDraw_EndPrims();
    RadarDraw_ClearPrimClip();
}

void BackdropLayer::DrawBorder(const RadarFrame& frame)
{
    ncl_math::Color rim{230, 220, 150};

    if (frame.ctx.highlight_team == RadarTeam::Terrorist)
    {
        rim = {235, 215, 95};
    }
    else if (frame.ctx.highlight_team == RadarTeam::CounterTerrorist)
    {
        rim = {120, 190, 255};
    }

    const uint8_t ca = 255;
    const int thickness = 1;

    if (frame.shape.round)
    {
        float outer = frame.shape.radius;
        float inner = outer - (float)thickness - 0.5f;
        RadarDraw_DrawBorderRing(frame.shape.center, outer, inner, 1.0f, rim.WithAlpha(ca));
        return;
    }

    int x1 = (int)frame.box.x;
    int y1 = (int)frame.box.y;
    int w = (int)frame.box.size;

    ncl_math::Color f = RadarDraw_ApplyFade(rim);
    gEngfuncs.pfnFillRGBABlend(x1, y1, w, thickness, f.r, f.g, f.b, ca);
    gEngfuncs.pfnFillRGBABlend(x1, y1 + w - thickness, w, thickness, f.r, f.g, f.b, ca);
    gEngfuncs.pfnFillRGBABlend(x1, y1, thickness, w, f.r, f.g, f.b, ca);
    gEngfuncs.pfnFillRGBABlend(x1 + w - thickness, y1, thickness, w, f.r, f.g, f.b, ca);
}
