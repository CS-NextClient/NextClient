#include "MapRender.h"

#include <algorithm>
#include <span>
#include <cmath>
#include <cstdint>

#include "main.h"

#include <ncl_gl/loader.h>
#include <glad/glad.h>
#include <goldsrc/engine/model.h>

#include "hud/radar/map/MapImageTone.h"
#include "hud/radar/map/MapRenderState.h"

namespace
{
    constexpr int kMaxSky = 1024;
    constexpr float kSkyGap = 64.0f;

    bool EnsureGlad()
    {
        return ncl_gl::EnsureLoaded();
    }

    // Surfedge sign is the edge direction: positive takes the first vertex, negative the second.
    const float* GetSurfVertex(goldsrc::model_t* world, goldsrc::msurface_t* s, int e)
    {
        int se = world->surfedges[s->firstedge + e];
        int v = se > 0 ? world->edges[se].v[0] : world->edges[-se].v[1];

        return world->vertexes[v].position;
    }

    float ComputeSurfaceUpZ(const goldsrc::msurface_t* s)
    {
        float nz = s->plane->normal[2];

        if (s->flags & goldsrc::SURF_PLANEBACK)
        {
            nz = -nz;
        }

        return nz;
    }

    // Mappers cap roofs with the skybox, so a surface with a sky face within kSkyGap above it is a
    // roof hiding the interior and is dropped; open areas have the skybox far overhead and stay.
    struct SkyFace
    {
        float z;
        ncl_math::Aabb2 box;
    };

    int CollectSkyFaces(goldsrc::model_t* world, std::span<SkyFace> out)
    {
        int nsky = 0;

        for (int i = 0; i < world->numsurfaces && nsky < (int)out.size(); i++)
        {
            goldsrc::msurface_t* s = &world->surfaces[i];

            if (!(s->flags & goldsrc::SURF_DRAWSKY) || !s->plane)
            {
                continue;
            }

            if (std::fabs(s->plane->normal[2]) < 0.5f)
            {
                continue;
            }

            SkyFace f{0.0f, ncl_math::Aabb2::Empty()};
            int nv = 0;

            for (int e = 0; e < s->numedges; e++)
            {
                const float* p = GetSurfVertex(world, s, e);
                f.z += p[2];
                f.box.Include({p[0], p[1]});
                nv++;
            }

            if (nv > 0)
            {
                f.z /= nv;
                out[nsky++] = f;
            }
        }

        return nsky;
    }

    bool IsSkyCapped(std::span<const SkyFace> skies, float cx, float cy, float topz)
    {
        for (const SkyFace& f : skies)
        {

            if (f.box.Contains({cx, cy}) && f.z >= topz - 2.0f && f.z <= topz + kSkyGap)
            {
                return true;
            }
        }

        return false;
    }

    // Not the world Z bounds: sky and pits squash every floor into a flat band and wash out the shading.
    void ComputeFloorZRange(goldsrc::model_t* world, std::span<const int> excluded, float& out_min, float& out_max)
    {
        float fzmin = 1.0e9f;
        float fzmax = -1.0e9f;

        for (int i = 0; i < world->numsurfaces; i++)
        {
            goldsrc::msurface_t* s = &world->surfaces[i];

            if (std::binary_search(excluded.begin(), excluded.end(), i) || (s->flags & (goldsrc::SURF_DRAWSKY | goldsrc::SURF_DRAWTILED)) ||
                !s->plane)
            {
                continue;
            }

            if (ComputeSurfaceUpZ(s) < 0.0f)
            {
                continue;
            }

            for (int e = 0; e < s->numedges; e++)
            {
                float z = GetSurfVertex(world, s, e)[2];
                fzmin = std::min(fzmin, z);
                fzmax = std::max(fzmax, z);
            }
        }

        out_min = fzmin;
        out_max = fzmax;
    }

    void DrawSurfaces(
        goldsrc::model_t* world,
        bool textured,
        const int (*lightmap_textures)[64],
        std::span<const SkyFace> skies,
        float fzmin,
        float zrange,
        std::span<const int> excluded
    )
    {
        for (int i = 0; i < world->numsurfaces; i++)
        {
            goldsrc::msurface_t* s = &world->surfaces[i];

            if (std::binary_search(excluded.begin(), excluded.end(), i) || (s->flags & (goldsrc::SURF_DRAWSKY | goldsrc::SURF_DRAWTILED)))
            {
                continue;
            }

            if (!s->plane)
            {
                continue;
            }

            if (textured && (!s->texinfo || !s->texinfo->texture))
            {
                continue;
            }

            if (ComputeSurfaceUpZ(s) < 0.0f)
            {
                continue;
            }

            {
                float cx = 0.0f;
                float cy = 0.0f;
                float topz = -1.0e9f;
                int nv = 0;

                for (int e = 0; e < s->numedges; e++)
                {
                    const float* p = GetSurfVertex(world, s, e);
                    cx += p[0];
                    cy += p[1];
                    topz = std::max(topz, p[2]);
                    nv++;
                }

                if (nv > 0)
                {
                    cx /= nv;
                    cy /= nv;

                    if (IsSkyCapped(skies, cx, cy, topz))
                    {
                        continue;
                    }
                }
            }

            if (textured)
            {
                int lm = (*lightmap_textures && s->lightmaptexturenum >= 0) ? (*lightmap_textures)[s->lightmaptexturenum] : 0;
                glActiveTexture(GL_TEXTURE1);

                if (lm)
                {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, lm);
                }
                else
                {
                    glDisable(GL_TEXTURE_2D);
                }

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, s->texinfo->texture->gl_texturenum);

                for (goldsrc::glpoly_t* poly = s->polys; poly; poly = poly->next)
                {
                    glBegin(GL_POLYGON);

                    for (int k = 0; k < poly->numverts; k++)
                    {
                        float* vt = poly->verts[k];
                        float shade = ncl_math::Saturate((vt[2] - fzmin) / zrange);
                        float c = ncl_math::Lerp(0.55f, 1.0f, shade);
                        glColor4f(c, c, c, 1.0f);
                        glMultiTexCoord2f(GL_TEXTURE0, vt[3], vt[4]);
                        glMultiTexCoord2f(GL_TEXTURE1, vt[5], vt[6]);
                        glVertex3f(vt[0], vt[1], vt[2]);
                    }

                    glEnd();
                }
            }
            else
            {
                glBegin(GL_POLYGON);

                for (int e = 0; e < s->numedges; e++)
                {
                    const float* p = GetSurfVertex(world, s, e);
                    float shade = ncl_math::Saturate((p[2] - fzmin) / zrange);
                    float c = ncl_math::Lerp(kMapFlatMinBrightness, kMapFlatMaxBrightness, shade);
                    glColor4f(c * 0.82f, c, c * 0.82f, 1.0f);
                    glVertex3f(p[0], p[1], p[2]);
                }

                glEnd();
            }
        }
    }

    struct RenderTarget
    {
        GLuint tex;
        GLuint fbo;
        GLuint rbo;
    };

    // The atlas is unbuilt for the first frames after level load: rendering then multiplies by
    // stale ids and comes out black. False means retry on a later frame.
    bool IsLightmapAtlasReady(goldsrc::model_t* world, const int (*lightmap_textures)[64])
    {
        if (!lightmap_textures)
        {
            return false;
        }

        for (int i = 0; i < world->numsurfaces; i++)
        {
            goldsrc::msurface_t* s = &world->surfaces[i];

            if ((s->flags & (goldsrc::SURF_DRAWSKY | goldsrc::SURF_DRAWTILED)) || !s->plane || s->lightmaptexturenum < 0)
            {
                continue;
            }

            if (glIsTexture((*lightmap_textures)[s->lightmaptexturenum]))
            {
                return true;
            }
        }

        return false;
    }

    bool ComputeMapExtent(goldsrc::model_t* world, ncl_math::Aabb3& out)
    {
        ncl_math::Aabb3 bounds{
            {world->mins[0], world->mins[1], world->mins[2]},
            {world->maxs[0], world->maxs[1], world->maxs[2]}
        };
        ncl_math::Vector3 center = bounds.Center();
        ncl_math::Vector3 size = bounds.Size();

        float half_span = std::max(size.x, size.y) * 0.5f * 1.04f;

        if (half_span < 1.0f)
        {
            return false;
        }

        out = {
            {center.x - half_span, center.y - half_span, bounds.min.z},
            {center.x + half_span, center.y + half_span, bounds.max.z}
        };

        if (out.max.z - out.min.z < 1.0f)
        {
            out.max.z = out.min.z + 1.0f;
        }

        return true;
    }

    void ReleaseRenderTarget(GLuint fbo, GLuint rbo)
    {
        glDeleteFramebuffers(1, &fbo);
        glDeleteRenderbuffers(1, &rbo);
    }

    bool AcquireRenderTarget(RenderTarget& out)
    {
        out = RenderTarget{};
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &out.tex);
        glBindTexture(GL_TEXTURE_2D, out.tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kMapImageSize, kMapImageSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenRenderbuffers(1, &out.rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, out.rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kMapImageSize, kMapImageSize);

        glGenFramebuffers(1, &out.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, out.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, out.tex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, out.rbo);

        if (out.tex && out.rbo && out.fbo && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE &&
            glGetError() == GL_NO_ERROR)
        {
            return true;
        }

        ReleaseRenderTarget(out.fbo, out.rbo);
        glDeleteTextures(1, &out.tex);
        out = RenderTarget{};
        return false;
    }

    void BeginTopDownPass(const ncl_math::Aabb3& extent, bool textured, bool slice)
    {
        glUseProgram(0);
        glViewport(0, 0, kMapImageSize, kMapImageSize);
        glActiveTexture(GL_TEXTURE1);
        glDisable(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE0);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_FOG);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        // Default keeps the topmost floor per pixel; slice flips it to keep the lowest, so roofs
        // lose to the passages beneath them without dropping any geometry.
        glDepthFunc(slice ? GL_GEQUAL : GL_LEQUAL);
        glClearDepth(slice ? 0.0 : 1.0);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(extent.min.x, extent.max.x, extent.min.y, extent.max.y, -extent.max.z, -extent.min.z);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (!textured)
        {
            glDisable(GL_TEXTURE_2D);
            return;
        }

        glActiveTexture(GL_TEXTURE1);
        glEnable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }

} // namespace

RuntimeMap MapRender_Generate(
    goldsrc::model_t* world,
    bool textured,
    bool slice,
    RadarMapStatus* status,
    std::span<const int> excluded_surfaces
)
{
    RuntimeMap rm{};

    if (status)
    {
        *status = RadarMapStatus::Waiting;
    }

    if (!world || world->numsurfaces <= 0 || !world->surfaces)
    {
        return rm;
    }

    if (!EnsureGlad() || !glGenFramebuffers || !glBindFramebuffer)
    {
        if (status)
        {
            *status = RadarMapStatus::Failed;
        }

        return rm;
    }

    const int (*lightmap_textures)[64] = nullptr;

    if (textured)
    {
        lightmap_textures = eng()->lightmap_textures;

        if (!IsLightmapAtlasReady(world, lightmap_textures))
        {
            return rm;
        }
    }

    ncl_math::Aabb3 extent;

    if (!ComputeMapExtent(world, extent))
    {
        if (status)
        {
            *status = RadarMapStatus::Failed;
        }

        return rm;
    }

    MapRenderState saved;

    RenderTarget target;

    if (!AcquireRenderTarget(target))
    {
        if (status)
        {
            *status = RadarMapStatus::Failed;
        }

        return rm;
    }

    BeginTopDownPass(extent, textured, slice);

    float fzmin;
    float fzmax;
    ComputeFloorZRange(world, excluded_surfaces, fzmin, fzmax);

    if (fzmax - fzmin < 1.0f)
    {
        fzmin = extent.min.z;
        fzmax = extent.max.z;
    }

    SkyFace skies[kMaxSky];
    int nsky = CollectSkyFaces(world, skies);

    DrawSurfaces(world, textured, lightmap_textures, {skies, (size_t)nsky}, fzmin, fzmax - fzmin, excluded_surfaces);

    MapImageTone_PremultiplyAlpha();

    if (textured)
    {
        MapImageTone_BrightenTextured();
    }

    ReleaseRenderTarget(target.fbo, target.rbo);

    if (glGetError() != GL_NO_ERROR)
    {
        glDeleteTextures(1, &target.tex);

        if (status)
        {
            *status = RadarMapStatus::Failed;
        }

        return rm;
    }

    rm.texture = target.tex;
    rm.bounds = {{extent.min.x, extent.min.y}, {extent.max.x, extent.max.y}};
    rm.valid = true;

    if (status)
    {
        *status = RadarMapStatus::Ready;
    }

    return rm;
}

void MapRender_Free(RuntimeMap& map)
{
    if (map.texture && EnsureGlad())
    {
        glDeleteTextures(1, reinterpret_cast<GLuint*>(&map.texture));
    }

    map = RuntimeMap{};
}
