#include "MapRender.h"

#include <algorithm>
#include <span>
#include <cmath>
#include <cstdint>

#include "main.h"

#include <ncl_gl/loader.h>
#include <glad/glad.h>
#include <Windows.h>
#include <goldsrc/engine/model.h>

namespace
{
    constexpr int kSize = 1024;
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
    void ComputeFloorZRange(goldsrc::model_t* world, float& out_min, float& out_max)
    {
        float fzmin = 1.0e9f, fzmax = -1.0e9f;

        for (int i = 0; i < world->numsurfaces; i++)
        {
            goldsrc::msurface_t* s = &world->surfaces[i];

            if ((s->flags & (goldsrc::SURF_DRAWSKY | goldsrc::SURF_DRAWTILED)) || !s->plane)
            {
                continue;
            }

            if (ComputeSurfaceUpZ(s) < -0.5f)
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
        float zrange
    )
    {
        for (int i = 0; i < world->numsurfaces; i++)
        {
            goldsrc::msurface_t* s = &world->surfaces[i];

            if (s->flags & (goldsrc::SURF_DRAWSKY | goldsrc::SURF_DRAWTILED))
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

            if (ComputeSurfaceUpZ(s) < -0.5f)
            {
                continue;
            }

            {
                float cx = 0.0f, cy = 0.0f, topz = -1.0e9f;
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
                    float c = ncl_math::Lerp(0.22f, 0.80f, shade);
                    glColor4f(c * 0.82f, c, c * 0.82f, 1.0f);
                    glVertex3f(p[0], p[1], p[2]);
                }

                glEnd();
            }
        }
    }

    struct SavedGlState
    {
        GLint unit;
        GLint tex;
        GLfloat clear_color[4];
        GLint viewport[4];
        GLboolean depth_mask;
        GLboolean blend;
        GLint fbo;
    };

    struct MapExtent
    {
        float minx, maxx;
        float miny, maxy;
        float minz, maxz;
    };

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

    bool ComputeMapExtent(goldsrc::model_t* world, MapExtent& out)
    {
        float cx = (world->mins[0] + world->maxs[0]) * 0.5f;
        float cy = (world->mins[1] + world->maxs[1]) * 0.5f;
        float span = std::max(world->maxs[0] - world->mins[0], world->maxs[1] - world->mins[1]) * 0.5f * 1.04f;

        if (span < 1.0f)
        {
            return false;
        }

        out.minx = cx - span;
        out.maxx = cx + span;
        out.miny = cy - span;
        out.maxy = cy + span;
        out.minz = world->mins[2];
        out.maxz = world->maxs[2];

        if (out.maxz - out.minz < 1.0f)
        {
            out.maxz = out.minz + 1.0f;
        }

        return true;
    }

    void SaveGlState(SavedGlState& out)
    {
        out.unit = GL_TEXTURE0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &out.unit);
        glActiveTexture(GL_TEXTURE0);
        out.tex = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &out.tex);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, out.clear_color);
        glGetIntegerv(GL_VIEWPORT, out.viewport);
        out.depth_mask = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &out.depth_mask);
        out.blend = glIsEnabled(GL_BLEND);
        out.fbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &out.fbo);
    }

    // Does not touch the colour texture: the caller keeps it on success, deletes it on failure.
    // Unit 0 goes back to the caller's binding, not zero: the engine caches the bound name and
    // early-outs on a hit, so a foreign binding left here makes its next HUD draw untextured.
    void ReleaseRenderTarget(GLint prev_fbo, GLuint fbo, GLuint rbo, GLint prev_tex)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
        glDeleteFramebuffers(1, &fbo);
        glDeleteRenderbuffers(1, &rbo);
        glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
    }

    // On failure nothing leaks: the colour texture is deleted here too, so the caller only has
    // to put the saved state back.
    bool AcquireRenderTarget(const SavedGlState& saved, RenderTarget& out)
    {
        out = RenderTarget{};

        glGenTextures(1, &out.tex);
        glBindTexture(GL_TEXTURE_2D, out.tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenRenderbuffers(1, &out.rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, out.rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kSize, kSize);

        glGenFramebuffers(1, &out.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, out.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, out.tex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, out.rbo);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        {
            return true;
        }

        ReleaseRenderTarget(saved.fbo, out.fbo, out.rbo, saved.tex);
        glDeleteTextures(1, &out.tex);
        out = RenderTarget{};

        return false;
    }

    // No return may sit between this and EndTopDownPass plus the saved-state restore after it.
    void BeginTopDownPass(const MapExtent& extent, bool textured, bool slice)
    {
        glUseProgram(0);
        glViewport(0, 0, kSize, kSize);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        // Default keeps the topmost floor per pixel; slice flips it to keep the lowest, so roofs
        // lose to the passages beneath them without dropping any geometry.
        glDepthFunc(slice ? GL_GEQUAL : GL_LEQUAL);
        glClearDepth(slice ? 0.0 : 1.0);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(extent.minx, extent.maxx, extent.miny, extent.maxy, -extent.maxz, -extent.minz);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
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

    void EndTopDownPass(const SavedGlState& saved, bool textured)
    {
        if (textured)
        {
            glActiveTexture(GL_TEXTURE1);
            glDisable(GL_TEXTURE_2D);
            glActiveTexture(GL_TEXTURE0);
        }

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glViewport(saved.viewport[0], saved.viewport[1], saved.viewport[2], saved.viewport[3]);
        glDisable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glClearDepth(1.0);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glEnable(GL_TEXTURE_2D);
        glDepthMask(saved.depth_mask);

        if (saved.blend)
        {
            glEnable(GL_BLEND);
        }
        else
        {
            glDisable(GL_BLEND);
        }
    }

    void RestoreGlState(const SavedGlState& saved)
    {
        glClearColor(saved.clear_color[0], saved.clear_color[1], saved.clear_color[2], saved.clear_color[3]);
        glActiveTexture((GLenum)saved.unit);
    }
} // namespace

RuntimeMap MapRender_Generate(goldsrc::model_t* world, bool textured, bool slice)
{
    RuntimeMap rm{};

    if (!world || world->numsurfaces <= 0 || !world->surfaces || !EnsureGlad())
    {
        return rm;
    }

    if (!glGenFramebuffers || !glBindFramebuffer)
    {
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

    MapExtent extent;

    if (!ComputeMapExtent(world, extent))
    {
        return rm;
    }

    SavedGlState saved;
    SaveGlState(saved);

    RenderTarget target;

    if (!AcquireRenderTarget(saved, target))
    {
        RestoreGlState(saved);
        return rm;
    }

    BeginTopDownPass(extent, textured, slice);

    float fzmin, fzmax;
    ComputeFloorZRange(world, fzmin, fzmax);

    if (fzmax - fzmin < 1.0f)
    {
        fzmin = extent.minz;
        fzmax = extent.maxz;
    }

    SkyFace skies[kMaxSky];
    int nsky = CollectSkyFaces(world, skies);

    DrawSurfaces(world, textured, lightmap_textures, {skies, (size_t)nsky}, fzmin, fzmax - fzmin);

    EndTopDownPass(saved, textured);

    ReleaseRenderTarget(saved.fbo, target.fbo, target.rbo, saved.tex);
    RestoreGlState(saved);

    rm.texture = target.tex;
    rm.bounds = {{extent.minx, extent.miny}, {extent.maxx, extent.maxy}};
    rm.valid = true;

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
