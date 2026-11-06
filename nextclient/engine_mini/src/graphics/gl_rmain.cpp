#include "engine.h"

#include <cmath>

#include <ncl_perf/profiler.h>
#include <next_engine_mini/MaterialEditInterface.h>
#include <common/mem.h>
#include <goldsrc/cvar.h>
#include <goldsrc/engine/client_state.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/engine/video.h>
#include <goldsrc/entity.h>
#include <goldsrc/studio_api.h>
#include <goldsrc/types.h>
#include <goldsrc/view.h>

#include "gl_local.h"
#include "gl_draw.h"
#include "graphics/render/material/material_edit.h"

#include "graphics/render/material/material_textures.h"
#include "graphics/render/render_capture.h"
#include "graphics/render/render_test.h"
#include "graphics/render/StudioDraw.h"
#include "graphics/render/effects/studio_effects.h"
#include "graphics/render/StudioPhaseCapture.h"
#include "graphics/render/StudioDump.h"
#include "graphics/render/model/studio_model_cache.h"
#include "graphics/render/RendererLifecycle.h"
#include "graphics/render/StudioIntercept.h"
#include "ViewmodelFrustumCalculator.h"
#include "console/console.h"
#include "common/sys_dll.h"
#include "performance/PerformanceMonitor.h"
#include "common/cvar.h"
#include "client/cl_main.h"

ViewmodelFrustumCalculator g_ViewmodelFrustumCalculator;

static bool IsVec3Finite(const vec3_t v)
{
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

int CL_FxBlend(goldsrc::cl_entity_t* e)
{
    NCL_PROFILE_SCOPE();

    return eng()->CL_FxBlend.InvokeChained(e);
}

float GlowBlend(goldsrc::cl_entity_t* pEntity)
{
    NCL_PROFILE_SCOPE();

    // the engine traces to r_entorigin, which R_SetupAttachmentPoint moves to an attachment point when curstate.body != 0
    if (!IsVec3Finite(r_refdef->vieworg) || !IsVec3Finite(*p_r_entorigin))
        return 0.0f;

    return eng()->GlowBlend.InvokeChained(pEntity);
}

void R_AllowFog(goldsrc::qboolean allow)
{
    if (allow)
    {
        if (*p_isFogEnabled)
            qglEnable(GL_FOG);
    }
    else
    {
        *p_isFogEnabled = qglIsEnabled(GL_FOG);
        if (*p_isFogEnabled)
            qglDisable(GL_FOG);
    }
}

float* R_GetAttachmentPoint(int entity, int attachment)
{
    NCL_PROFILE_SCOPE();

    return eng()->R_GetAttachmentPoint.InvokeChained(entity, attachment);
}

void R_SetupAttachmentPoint(goldsrc::cl_entity_t* ent)
{
    if (ent->curstate.body)
    {
        float* attachment_pont = R_GetAttachmentPoint(ent->curstate.skin, ent->curstate.body);
        (*p_r_entorigin)[0] = attachment_pont[0];
        (*p_r_entorigin)[1] = attachment_pont[1];
        (*p_r_entorigin)[2] = attachment_pont[2];
    }
    else
    {
        (*p_r_entorigin)[0] = ent->origin[0];
        (*p_r_entorigin)[1] = ent->origin[1];
        (*p_r_entorigin)[2] = ent->origin[2];
    }
}

void R_SetProjectionMatrixForViewmodelFov()
{
    qglMatrixMode(GL_PROJECTION);
    qglPushMatrix();
    qglLoadIdentity();

    GLdouble left, right, bottom, top;
    g_ViewmodelFrustumCalculator.CalcFrustum(left, right, bottom, top);

    qglFrustum(left, right, bottom, top, ViewmodelFrustumCalculator::kZNear, ViewmodelFrustumCalculator::kZFar);

    qglMatrixMode(GL_MODELVIEW);
}

void R_RestoreProjectionMatrix()
{
    qglMatrixMode(GL_PROJECTION);
    qglPopMatrix();
    qglMatrixMode(GL_MODELVIEW);
}

void AddTEntity(goldsrc::cl_entity_t* ent)
{
    NCL_PROFILE_SCOPE();

    return eng()->AddTEntity.InvokeChained(ent);
}

void AppendTEntity_Subscriber(goldsrc::cl_entity_t* ent)
{
    // Tag only the local viewmodel's flash (the temp entity, not cl.viewent) so other players'
    // flashes keep the world FOV. skin is unused by sprite rendering and independent of viewmodel fx.
    if (*p_currententity == &cl->viewent)
    {
        ent->curstate.skin = cl->viewent.index;
    }
}

void R_DestroyObjects()
{
    NCL_PROFILE_SCOPE();

    if (*p_transObjects)
    {
        Mem_Free(*p_transObjects);
        *p_transObjects = nullptr;
    }
    
    *p_maxTransObjs = 0;
}

void R_DrawSpriteModel(goldsrc::cl_entity_t* e)
{
    NCL_PROFILE_SCOPE();

    eng()->R_DrawSpriteModel.InvokeChained(e);
}

void R_DrawViewmodelSpriteModel(goldsrc::cl_entity_t* e)
{
    GLdouble depth_range[2];
    qglGetDoublev(GL_DEPTH_RANGE, depth_range);

    R_SetProjectionMatrixForViewmodelFov();
    qglDepthRange(depth_range[0], (depth_range[1] - depth_range[0]) * 0.3 + depth_range[0]);
    R_DrawSpriteModel(e);
    qglDepthRange(depth_range[0], depth_range[1]);
    R_RestoreProjectionMatrix();
}

void R_DrawBrushModel(goldsrc::cl_entity_t* e)
{
    NCL_PROFILE_SCOPE();

    eng()->R_DrawBrushModel.InvokeChained(e);
}

void R_DrawAliasModel(goldsrc::cl_entity_t* e)
{
    NCL_PROFILE_SCOPE();

    eng()->R_DrawAliasModel.InvokeChained(e);
}

void R_NewMap()
{
    NCL_PROFILE_SCOPE();

    render::GetLifecycle().OnMapReset();

    // Force a skybox reload: on reconnect to a cached (same-map) BSP, R_InitSky is skipped and gLoadSky stays false,
    // so R_LoadSkys would wipe the sky without reloading it (white sky).
    // TODO: remove this once the texture/model cache consistency issue is fixed 
    // (brush models reloading on session change re-runs R_InitSky and makes this redundant).
    if (eng()->gLoadSky != nullptr)
    {
        *eng()->gLoadSky = TRUE;
    }

    eng()->R_NewMap.InvokeChained();
}

void R_SetupFrame()
{
    NCL_PROFILE_SCOPE();

    eng()->R_SetupFrame.InvokeChained();
}

void R_SetFrustum()
{
    NCL_PROFILE_SCOPE();

    eng()->R_SetFrustum.InvokeChained();
}

void R_SetupGL()
{
    NCL_PROFILE_SCOPE();

    eng()->R_SetupGL.InvokeChained();
}

void R_MarkLeaves()
{
    NCL_PROFILE_SCOPE();

    eng()->R_MarkLeaves.InvokeChained();
}

void R_DrawWorld()
{
    NCL_PROFILE_SCOPE();

    const uint64_t started = render::StudioPhase_BeginWorld();

    eng()->R_DrawWorld.InvokeChained();
    render::StudioPhase_EndWorld(started);
}

void R_RenderFinalFog()
{
    NCL_PROFILE_SCOPE();

    eng()->R_RenderFinalFog.InvokeChained();
}

void R_RenderDlights()
{
    NCL_PROFILE_SCOPE();

    eng()->R_RenderDlights.InvokeChained();
}

void R_DrawParticles()
{
    NCL_PROFILE_SCOPE();

    eng()->R_DrawParticles.InvokeChained();
}

void R_DrawViewModel()
{
    NCL_PROFILE_SCOPE();
    eng()->R_DrawViewModel.InvokeChained();
}

void R_PolyBlend()
{
    NCL_PROFILE_SCOPE();

    eng()->R_PolyBlend.InvokeChained();
}

void R_PreDrawViewModel()
{
    NCL_PROFILE_SCOPE();
    eng()->R_PreDrawViewModel.InvokeChained();
}

void R_Clear()
{
    NCL_PROFILE_SCOPE();

    eng()->R_Clear.InvokeChained();
}

void R_DrawStudioModel(goldsrc::cl_entity_t* ent,
                       const std::function<goldsrc::cl_entity_t*(int index)>& ent_list_getter,
                       int ent_list_count)
{
    NCL_PROFILE_SCOPE();

    if (ent->player)
    {
        render::StudioPlayerEffectsScope effects(ent);
        pStudioAPI->StudioDrawPlayer(goldsrc::STUDIO_RENDER | goldsrc::STUDIO_EVENTS, &ent->curstate);
        render::Studio_EndModel();
        return;
    }

    if (ent->curstate.movetype == goldsrc::MOVETYPE_FOLLOW)
    {
        NCL_PROFILE_NAMED_SCOPE("Studio follow traversal");

        for (int i = 0; i < ent_list_count; i++)
        {
            goldsrc::cl_entity_t* e = ent_list_getter(i);

            if (e->index == ent->curstate.aiment)
            {
                *p_currententity = e;
                {
                    NCL_PROFILE_NAMED_SCOPE("Studio parent pose");
                    if ((*p_currententity)->player)
                    {
                        pStudioAPI->StudioDrawPlayer(0, &(*p_currententity)->curstate);
                    }
                    else
                    {
                        pStudioAPI->StudioDrawModel(0);
                    }
                }
                *p_currententity = ent;
                pStudioAPI->StudioDrawModel(goldsrc::STUDIO_RENDER | goldsrc::STUDIO_EVENTS);
            }
        }
    }
    else
    {
        pStudioAPI->StudioDrawModel(goldsrc::STUDIO_RENDER | goldsrc::STUDIO_EVENTS);
    }

    render::Studio_EndModel();
}

void R_DrawTEntitiesOnList(goldsrc::qboolean clientOnly)
{
    NCL_PROFILE_SCOPE();

    if (r_drawentities->value == 0.0)
        return;

    if (clientOnly == false)
    {
        render::Studio_BeginTransparentPass();
        render::StudioPhase_BeginPass(2);

        for (int i = 0; i < *p_numTransObjs; ++i)
        {
            goldsrc::cl_entity_t* ent = (*p_transObjects)[i].pEnt;
            *p_currententity = ent;

            qglDisable(GL_FOG);
            *p_r_blend = (GLfloat)CL_FxBlend(ent);

            if (*p_r_blend > 0.0)
            {
                *p_r_blend *= 1.f / 255.f;

                if (ent->model->type != goldsrc::mod_sprite && ent->curstate.rendermode == goldsrc::kRenderGlow)
                    Con_DPrintf(ConLogType::Info, "Non-sprite set to glow!\n");

                switch (ent->model->type)
                {
                    case goldsrc::mod_sprite:
                        render::Studio_Flush(render::StudioFlushReason::Client);
                        R_SetupAttachmentPoint(ent);

                        if (ent->curstate.rendermode == goldsrc::kRenderGlow)
                            *p_r_blend = GlowBlend(ent) * (*p_r_blend);

                        if (*p_r_blend != 0.0)
                        {
                            if (cl->viewent.index != 0 && cl->viewent.index == ent->curstate.skin)
                            {
                                R_DrawViewmodelSpriteModel(ent);
                            }
                            else
                            {
                                R_DrawSpriteModel(ent);
                            }
                        }
                        break;

                    case goldsrc::mod_brush:
                        render::Studio_Flush(render::StudioFlushReason::Brush);
                        if (*p_g_bUserFogOn)
                        {
                            if (ent->curstate.rendermode != goldsrc::kRenderGlow && ent->curstate.rendermode != goldsrc::kRenderTransAdd)
                                qglEnable(GL_FOG);
                        }
                        R_DrawBrushModel(ent);
                        break;

                    case goldsrc::mod_alias:
                        render::Studio_Flush(render::StudioFlushReason::Client);
                        R_DrawAliasModel(ent);
                        break;

                    case goldsrc::mod_studio:
                        if (!ent->curstate.renderamt)
                            break;

                        R_DrawStudioModel(ent, [](int index) { return (*p_transObjects)[index].pEnt; } , *p_numTransObjs);
                        break;

                    default:
                        break;
                }
            }
        }

        render::Studio_EndTransparentPass();
        render::StudioPhase_EndPass();
    }

    GL_DisableMultitexture();
    qglEnable(GL_ALPHA_TEST);
    qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    if (*p_g_bUserFogOn)
        qglDisable(GL_FOG);
    ClientDLL_DrawTransparentTriangles();
    if (*p_g_bUserFogOn)
        qglEnable(GL_FOG);
    *p_r_blend = 1.0;

    *p_numTransObjs = 0;
}

void R_DrawEntitiesOnList()
{
    NCL_PROFILE_SCOPE();

    if (r_drawentities->value == 0.0)
        return;

    render::Studio_BeginOpaquePass();
    render::StudioPhase_BeginPass(1);

    for (int i = 0; i < *p_cl_numvisedicts; i++)
    {
        goldsrc::cl_entity_t* ent = p_cl_visedicts[i];
        *p_currententity = ent;

        if (ent->curstate.rendermode)
        {
            AddTEntity(ent);
            continue;
        }

        switch (ent->model->type)
        {
            case goldsrc::mod_brush:
                render::Studio_Flush(render::StudioFlushReason::Brush);
                R_DrawBrushModel(ent);
                break;

            case goldsrc::mod_studio:
                R_DrawStudioModel(ent, [](int index) { return p_cl_visedicts[index]; } , *p_cl_numvisedicts);
                break;

            default:
                break;
        }
    }

    render::Studio_EndOpaquePass();
    render::StudioPhase_EndPass();
    *p_r_blend = 1.0;

    for (int i = 0; i < *p_cl_numvisedicts; i++)
    {
        goldsrc::cl_entity_t* ent = p_cl_visedicts[i];
        *p_currententity = ent;

        if (ent->curstate.rendermode)
            continue;

        if (ent->model->type == goldsrc::mod_sprite)
        {
            R_SetupAttachmentPoint(ent);

            if (ent->curstate.body && cl->viewent.index == ent->curstate.skin)
            {
                R_DrawViewmodelSpriteModel(ent);
            }
            else
            {
                R_DrawSpriteModel(ent);
            }
        }
    }
}

void R_RenderScene()
{
    NCL_PROFILE_SCOPE();

    *p_r_blend = 1.0f;
    
    if (CL_IsDevOverviewMode())
        CL_SetDevOverView(r_refdef);

    R_SetupFrame();
    R_SetFrustum();
    R_SetupGL();
    R_MarkLeaves();

    if (!r_refdef->onlyClientDraws)
    {
        if (CL_IsDevOverviewMode())
        {
            qglClearColor(0, 1.f, 0, 0);
            qglClear(GL_COLOR_BUFFER_BIT);
        }
        R_DrawWorld();
        S_ExtraUpdate();
        R_DrawEntitiesOnList();
    }

    if (*p_g_bUserFogOn)
        R_RenderFinalFog();

    R_AllowFog(FALSE);
    ClientDLL_DrawNormalTriangles();
    R_AllowFog(TRUE);

    if (cl->waterlevel > 2 && !r_refdef->onlyClientDraws || !*p_g_bUserFogOn)
        qglDisable(GL_FOG);

    R_DrawTEntitiesOnList(r_refdef->onlyClientDraws);
    S_ExtraUpdate();

    if (!r_refdef->onlyClientDraws)
    {
        R_RenderDlights();
        GL_DisableMultitexture();
        R_DrawParticles();
    }
}

void R_RenderView()
{
    NCL_PROFILE_SCOPE();

    double time1 = 0;

    if (r_norefresh->value != 0.0)
        return;

    if (!p_r_worldentity->model || !cl->worldmodel)
        Sys_Error("%s: NULL worldmodel", __func__);

    if (r_speeds->value != 0.0)
    {
        qglFinish();
        time1 = Sys_FloatTime();
        *p_c_brush_polys = 0;
        *p_c_alias_polys = 0;
    }

    render::StudioPhase_BeginView();
    *p_mirror = false;

    R_Clear();
    if (!r_refdef->onlyClientDraws)
        R_PreDrawViewModel();

    render::StudioEffects_BeginView();
    R_RenderScene();
    render::Studio_Flush(render::StudioFlushReason::StageEnd);
    render::StudioEffects_EndView();

    if (!r_refdef->onlyClientDraws)
    {
        R_SetProjectionMatrixForViewmodelFov();
        R_DrawViewModel();
        R_RestoreProjectionMatrix();

        R_PolyBlend();
    }

    render::Studio_DiscardPending();
    render::Test_Draw();
    render::Dump_OnSceneEnd();
    render::Capture_OnSceneEnd();

    S_ExtraUpdate();

    if (r_speeds->value != 0.0)
    {
        double framerate = cl->time - cl->oldtime;
        if (framerate > 0.0)
            framerate = 1.0 / framerate;

        Con_Printf("%3ifps %3i ms  %4i wpoly %4i epoly\n",
            (int) lround(framerate + 0.5),
            (int) ((Sys_FloatTime() - time1) * 1000.0),
            *p_c_brush_polys,
            *p_c_alias_polys);
    }

    render::StudioPhase_EndView();
}

void R_ForceCVars(goldsrc::qboolean mp)
{
    if (!mp)
    {
        return;
    }

    if (r_lightmap->value != 0.0)
    {
        Cvar_DirectSet(r_lightmap, "0");
    }

    if (gl_clear->value != 0.0)
    {
        Cvar_DirectSet(gl_clear, "0");
    }

    if (r_novis->value != 0.0)
    {
        Cvar_DirectSet(r_novis, "0");
    }

    if (r_fullbright->value != 0.0)
    {
        Cvar_DirectSet(r_fullbright, "0");
    }

    if (snd_show->value != 0.0)
    {
        Cvar_DirectSet(snd_show, "0");
    }

    if (chase_active->value != 0.0)
    {
        Cvar_DirectSet(chase_active, "0");
    }

    if (lambert_cvar->value != 1.5)
    {
        Cvar_DirectSet(lambert_cvar, "1.5");
    }

    if (gl_monolights->value != 0.0)
    {
        Cvar_DirectSet(gl_monolights, "0");
        eng()->GL_BuildLightmaps();
    }

    if (gl_wireframe->value != 0.0)
    {
        Cvar_DirectSet(gl_wireframe, "0");
    }

    if (r_dynamic->value != 1.0)
    {
        Cvar_DirectSet(r_dynamic, "1");
    }

    if (gl_alphamin->value != 0.25)
    {
        Cvar_DirectSet(gl_alphamin, "0.25");
    }

    if (gl_max_size.value < 255.0)
    {
        Cvar_DirectSet(&gl_max_size, "255");
    }

    float value = gl_polyoffset->value;
    if (value <= 0.0)
    {
        if (value < -0.001)
        {
            Cvar_DirectSet(gl_polyoffset, "-0.001");
        }
    }
    else if (value < 0.001)
    {
        Cvar_DirectSet(gl_polyoffset, "0.001");
    }
    else if (gl_polyoffset->value > 25.0)
    {
        Cvar_DirectSet(gl_polyoffset, "25");
    }

    if (sv_cheats->value == 0.0 && r_drawentities->value != 1.0)
    {
        Cvar_DirectSet(r_drawentities, "1.0");
    }

    if (lightgamma_cvar->value < 1.8)
    {
        Cvar_DirectSet(lightgamma_cvar, "1.8");
        eng()->GL_BuildLightmaps();
    }
}
