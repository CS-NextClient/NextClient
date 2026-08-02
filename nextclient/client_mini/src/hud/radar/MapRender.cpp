#include "MapRender.h"
#include <ncl_gl/loader.h>
#include "../../main.h"

#include <glad/glad.h>
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    constexpr int kSize = 1024;
    constexpr int kMaxSky = 1024;
    constexpr float kSkyGap = 64.0f;

    bool EnsureGlad()
    {
        return ncl_gl::EnsureLoaded();
    }

    // Surfedge sign encodes the edge direction: positive selects the first vertex,
    // negative the second of the reversed edge.
    const float* SurfVertex(model_t* world, msurface_t* s, int e)
    {
        int se = world->surfedges[s->firstedge + e];
        int v = se > 0 ? world->edges[se].v[0] : world->edges[-se].v[1];

        return world->vertexes[v].position;
    }

    // Upward component of the surface normal, with the SURF_PLANEBACK side flip.
    float SurfaceUpZ(const msurface_t* s)
    {
        float nz = s->plane->normal[2];

        if (s->flags & SURF_PLANEBACK)
        {
            nz = -nz;
        }

        return nz;
    }

    // Roughly-horizontal sky faces. Mappers often cap roofs with the skybox, so a
    // surface that has a sky face right above it (within kSkyGap) is a roof hiding
    // what is under it: drop it. Open areas have the skybox far overhead and stay
    // untouched.
    struct SkyFace
    {
        float z;
        ncl_math::Aabb2 box;
    };

    int CollectSkyFaces(model_t* world, SkyFace* out, int max_out)
    {
        int nsky = 0;

        for (int i = 0; i < world->numsurfaces && nsky < max_out; i++)
        {
            msurface_t* s = &world->surfaces[i];

            if (!(s->flags & SURF_DRAWSKY) || !s->plane)
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
                const float* p = SurfVertex(world, s, e);
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

    bool SkyCapped(const SkyFace* skies, int nsky, float cx, float cy, float topz)
    {
        for (int k = 0; k < nsky; k++)
        {
            const SkyFace& f = skies[k];

            if (f.box.Contains({cx, cy}) && f.z >= topz - 2.0f && f.z <= topz + kSkyGap)
            {
                return true;
            }
        }

        return false;
    }

    // Z range spanned by upward-facing floor vertices, used to fit the height
    // shading. The world Z bounds include the sky and pits, which squash every
    // floor into a flat band and wash out the height differences.
    void ComputeFloorZRange(model_t* world, float* out_min, float* out_max)
    {
        float fzmin = 1.0e9f, fzmax = -1.0e9f;

        for (int i = 0; i < world->numsurfaces; i++)
        {
            msurface_t* s = &world->surfaces[i];

            if ((s->flags & (SURF_DRAWSKY | SURF_DRAWTILED)) || !s->plane)
            {
                continue;
            }

            if (SurfaceUpZ(s) < -0.5f)
            {
                continue;
            }

            for (int e = 0; e < s->numedges; e++)
            {
                float z = SurfVertex(world, s, e)[2];
                fzmin = std::min(fzmin, z);
                fzmax = std::max(fzmax, z);
            }
        }

        *out_min = fzmin;
        *out_max = fzmax;
    }

    // Draws every upward-facing, non-sky, non-sky-capped surface top-down: with the
    // map's own textures (times the lightmap on unit 1) or flat height-shaded.
    void DrawSurfaces(
        model_t* world,
        bool textured,
        const int (*lightmap_textures)[64],
        const SkyFace* skies,
        int nsky,
        float fzmin,
        float zrange
    )
    {
        for (int i = 0; i < world->numsurfaces; i++)
        {
            msurface_t* s = &world->surfaces[i];

            if (s->flags & (SURF_DRAWSKY | SURF_DRAWTILED))
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

            if (SurfaceUpZ(s) < -0.5f) // skip downward-facing surfaces (ceilings/roofs)
            {
                continue;
            }

            {
                float cx = 0.0f, cy = 0.0f, topz = -1.0e9f;
                int nv = 0;

                for (int e = 0; e < s->numedges; e++)
                {
                    const float* p = SurfVertex(world, s, e);
                    cx += p[0];
                    cy += p[1];
                    topz = std::max(topz, p[2]);
                    nv++;
                }

                if (nv > 0)
                {
                    cx /= nv;
                    cy /= nv;

                    if (SkyCapped(skies, nsky, cx, cy, topz))
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

                for (glpoly_t* poly = s->polys; poly; poly = poly->next)
                {
                    glBegin(GL_POLYGON);

                    for (int k = 0; k < poly->numverts; k++)
                    {
                        float* vt = poly->verts[k];
                        float shade = ncl_math::Saturate((vt[2] - fzmin) / zrange);
                        float c = ncl_math::Lerp(0.55f, 1.0f, shade); // gentle height cue over the lightmap
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
                    const float* p = SurfVertex(world, s, e);
                    float shade = ncl_math::Saturate((p[2] - fzmin) / zrange);
                    float c = ncl_math::Lerp(0.22f, 0.80f, shade);
                    glColor4f(c * 0.82f, c, c * 0.82f, 1.0f);
                    glVertex3f(p[0], p[1], p[2]);
                }

                glEnd();
            }
        }
    }

    // The texture is not touched here: the caller keeps it on success and deletes it
    // separately on failure.
    void ReleaseRenderTarget(GLint prev_fbo, GLuint fbo, GLuint rbo)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
        glDeleteFramebuffers(1, &fbo);
        glDeleteRenderbuffers(1, &rbo);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
} // namespace

RuntimeMap MapRender_Generate(model_t* world, bool textured, bool slice)
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

    // Lightmap atlas (engine global). On a fresh level the atlas is not built for
    // the first frames after load, so a textured render would multiply the base by
    // stale/invalid ids and come out black. Bail until at least one lightmap is a
    // real GL texture and let the caller retry.
    const int (*lightmap_textures)[64] = nullptr;

    if (textured)
    {
        lightmap_textures = eng()->lightmap_textures; // hw.dll lightmap atlas ids (via NitroApi)
        bool lm_ready = false;

        if (lightmap_textures)
        {
            for (int i = 0; i < world->numsurfaces; i++)
            {
                msurface_t* s = &world->surfaces[i];

                if ((s->flags & (SURF_DRAWSKY | SURF_DRAWTILED)) || !s->plane || s->lightmaptexturenum < 0)
                {
                    continue;
                }

                if (glIsTexture((*lightmap_textures)[s->lightmaptexturenum]))
                {
                    lm_ready = true;
                    break;
                }
            }
        }

        if (!lm_ready)
        {
            return rm;
        }
    }

    float cx = (world->mins[0] + world->maxs[0]) * 0.5f;
    float cy = (world->mins[1] + world->maxs[1]) * 0.5f;
    float span = std::max(world->maxs[0] - world->mins[0], world->maxs[1] - world->mins[1]) * 0.5f * 1.04f;

    if (span < 1.0f)
    {
        return rm;
    }

    float minx = cx - span, maxx = cx + span;
    float miny = cy - span, maxy = cy + span;
    float minz = world->mins[2], maxz = world->maxs[2];

    if (maxz - minz < 1.0f)
    {
        maxz = minz + 1.0f;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLuint rbo = 0;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kSize, kSize);

    GLint prev_fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        ReleaseRenderTarget(prev_fbo, fbo, rbo);
        glDeleteTextures(1, &tex);
        return rm;
    }

    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);
    // This runs mid-HUD-frame, so restore the state the engine relies on.
    GLboolean prev_depth_mask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &prev_depth_mask);
    GLboolean prev_blend = glIsEnabled(GL_BLEND);

    glUseProgram(0);
    glViewport(0, 0, kSize, kSize);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    // Default keeps the topmost floor per pixel (higher z is nearer). The slice
    // modes flip this to keep the lowest floor, so roofs and upper levels lose to
    // the interior passages beneath them without dropping any geometry.
    glDepthFunc(slice ? GL_GEQUAL : GL_LEQUAL);
    glClearDepth(slice ? 0.0 : 1.0);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(minx, maxx, miny, maxy, -maxz, -minz); // top-down, higher z is nearer
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (textured)
    {
        glActiveTexture(GL_TEXTURE1);
        glEnable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }
    else
    {
        glDisable(GL_TEXTURE_2D);
    }

    float fzmin, fzmax;
    ComputeFloorZRange(world, &fzmin, &fzmax);

    if (fzmax - fzmin < 1.0f)
    {
        fzmin = minz;
        fzmax = maxz;
    }

    float zrange = fzmax - fzmin;

    SkyFace skies[kMaxSky];
    int nsky = CollectSkyFaces(world, skies, kMaxSky);

    DrawSurfaces(world, textured, lightmap_textures, skies, nsky, fzmin, zrange);

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

    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(prev_depth_mask);

    if (prev_blend)
    {
        glEnable(GL_BLEND);
    }
    else
    {
        glDisable(GL_BLEND);
    }

    ReleaseRenderTarget(prev_fbo, fbo, rbo);

    rm.texture = tex;
    rm.bounds = {{minx, miny}, {maxx, maxy}};
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
