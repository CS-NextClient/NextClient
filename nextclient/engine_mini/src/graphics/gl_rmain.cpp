#include "engine.h"

#include <optick.h>
#include <common/mem.h>

#include "gl_local.h"
#include "gl_draw.h"
#include "render/MaterialEdit.h"
#include "render/MaterialTextures.h"
#include "render/RenderCapture.h"
#include "render/RenderTest.h"
#include "render/StudioDraw.h"
#include "render/StudioDump.h"
#include "render/StudioGeometry.h"
#include "render/StudioIntercept.h"
#include "ViewmodelFrustumCalculator.h"
#include "console/console.h"
#include "common/sys_dll.h"
#include "common/cvar.h"
#include "client/cl_main.h"

ViewmodelFrustumCalculator g_ViewmodelFrustumCalculator;

int CL_FxBlend(cl_entity_t* e)
{
    OPTICK_EVENT();

    return eng()->CL_FxBlend.InvokeChained(e);
}

float GlowBlend(cl_entity_t* pEntity)
{
    OPTICK_EVENT();

    return eng()->GlowBlend.InvokeChained(pEntity);
}

void R_AllowFog(qboolean allow)
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
    OPTICK_EVENT();

    return eng()->R_GetAttachmentPoint.InvokeChained(entity, attachment);
}

void R_SetupAttachmentPoint(cl_entity_t* ent)
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

void AddTEntity(cl_entity_t* ent)
{
    OPTICK_EVENT();

    return eng()->AddTEntity.InvokeChained(ent);
}

void AppendTEntity_Subscriber(cl_entity_t* ent)
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
    OPTICK_EVENT();

    if (*p_transObjects)
    {
        Mem_Free(*p_transObjects);
        *p_transObjects = nullptr;
    }
    
    *p_maxTransObjs = 0;
}

void R_DrawSpriteModel(cl_entity_t* e)
{
    OPTICK_EVENT();

    eng()->R_DrawSpriteModel.InvokeChained(e);
}

void R_DrawViewmodelSpriteModel(cl_entity_t* e)
{
    GLdouble depth_range[2];
    qglGetDoublev(GL_DEPTH_RANGE, depth_range);

    R_SetProjectionMatrixForViewmodelFov();
    qglDepthRange(depth_range[0], (depth_range[1] - depth_range[0]) * 0.3 + depth_range[0]);
    R_DrawSpriteModel(e);
    qglDepthRange(depth_range[0], depth_range[1]);
    R_RestoreProjectionMatrix();
}

void R_DrawBrushModel(cl_entity_t* e)
{
    OPTICK_EVENT();

    eng()->R_DrawBrushModel.InvokeChained(e);
}

void R_DrawAliasModel(cl_entity_t* e)
{
    OPTICK_EVENT();

    eng()->R_DrawAliasModel.InvokeChained(e);
}

void R_NewMap()
{
    OPTICK_EVENT();

    // Force a skybox reload: on reconnect to a cached (same-map) BSP, R_InitSky is skipped and gLoadSky stays false,
    // so R_LoadSkys would wipe the sky without reloading it (white sky).
    // TODO: remove this once the texture/model cache consistency issue is fixed 
    // (brush models reloading on session change re-runs R_InitSky and makes this redundant).
    if (eng()->gLoadSky != nullptr)
    {
        *eng()->gLoadSky = TRUE;
    }

    // The engine reuses its model slots across maps, so cached studio geometry keyed by
    // a model would otherwise survive into a map that loaded different models.
    render::Studio_ForgetModel();
    render::Geometry_Clear();
    render::MaterialTextures_Clear();

    eng()->R_NewMap.InvokeChained();
}

void R_SetupFrame()
{
    OPTICK_EVENT();

    eng()->R_SetupFrame.InvokeChained();
}

void R_SetFrustum()
{
    OPTICK_EVENT();

    eng()->R_SetFrustum.InvokeChained();
}

void R_SetupGL()
{
    OPTICK_EVENT();

    eng()->R_SetupGL.InvokeChained();
}

void R_MarkLeaves()
{
    OPTICK_EVENT();

    eng()->R_MarkLeaves.InvokeChained();
}

void R_DrawWorld()
{
    OPTICK_EVENT();

    eng()->R_DrawWorld.InvokeChained();
}

void R_RenderFinalFog()
{
    OPTICK_EVENT();

    eng()->R_RenderFinalFog.InvokeChained();
}

void R_RenderDlights()
{
    OPTICK_EVENT();

    eng()->R_RenderDlights.InvokeChained();
}

void R_DrawParticles()
{
    OPTICK_EVENT();

    eng()->R_DrawParticles.InvokeChained();
}

void R_DrawViewModel()
{
    OPTICK_EVENT();

    // Before the draw, which is where the engine poses the weapon from these two.
    render::MaterialEdit_ApplyViewmodel(&cl->viewent, cl->time, &cl->weaponsequence, &cl->weaponstarttime);

    eng()->R_DrawViewModel.InvokeChained();

    // Noted after the draw rather than before it: what the report is asked for is what the
    // renderer did with the state, and both the frame it settled on and the bones it built
    // exist only once it has run.
    render::MaterialEdit_NoteViewmodel(&cl->viewent, cl->time);
}

void R_PolyBlend()
{
    OPTICK_EVENT();

    eng()->R_PolyBlend.InvokeChained();
}

void R_PreDrawViewModel()
{
    OPTICK_EVENT();

    // This pass poses the weapon a first time and fires the animation's events while doing
    // it. Skipped while the panel drives, because an author standing on one frame of a reload
    // does not want that frame's sound fired on every frame the panel holds it. The pose it
    // would have built is built again by the draw itself, from the same two values.
    if (render::MaterialEdit_DrivesViewmodel())
    {
        return;
    }

    eng()->R_PreDrawViewModel.InvokeChained();
}

void R_Clear()
{
    OPTICK_EVENT();

    eng()->R_Clear.InvokeChained();
}

void R_DrawStudioModel(cl_entity_t* ent, const std::function<cl_entity_t*(int index)>& ent_list_getter, int ent_list_count)
{
    OPTICK_EVENT();

    if (ent->player)
    {
        pStudioAPI->StudioDrawPlayer(STUDIO_RENDER | STUDIO_EVENTS, &ent->curstate);
        return;
    }

    if (ent->curstate.movetype == MOVETYPE_FOLLOW)
    {
        for (int i = 0; i < ent_list_count; i++)
        {
            cl_entity_t* e = ent_list_getter(i);

            if (e->index == ent->curstate.aiment)
            {
                *p_currententity = e;
                if ((*p_currententity)->player)
                    pStudioAPI->StudioDrawPlayer(0, &(*p_currententity)->curstate);
                else
                    pStudioAPI->StudioDrawModel(0);
                *p_currententity = ent;
                pStudioAPI->StudioDrawModel(STUDIO_RENDER | STUDIO_EVENTS);
            }
        }
    }
    else
    {
        pStudioAPI->StudioDrawModel(STUDIO_RENDER | STUDIO_EVENTS);
    }
}

void R_DrawTEntitiesOnList(qboolean clientOnly)
{
    OPTICK_EVENT();

    if (r_drawentities->value == 0.0)
        return;

    if (clientOnly == false)
    {
        for (int i = 0; i < *p_numTransObjs; ++i)
        {
            cl_entity_t* ent = (*p_transObjects)[i].pEnt;
            *p_currententity = ent;

            qglDisable(GL_FOG);
            *p_r_blend = (GLfloat)CL_FxBlend(ent);

            if (*p_r_blend > 0.0)
            {
                *p_r_blend *= 1.f / 255.f;

                if (ent->model->type != mod_sprite && ent->curstate.rendermode == kRenderGlow)
                    Con_DPrintf(ConLogType::Info, "Non-sprite set to glow!\n");

                switch (ent->model->type)
                {
                    case mod_sprite:
                        R_SetupAttachmentPoint(ent);

                        if (ent->curstate.rendermode == kRenderGlow)
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

                    case mod_brush:
                        if (*p_g_bUserFogOn)
                        {
                            if (ent->curstate.rendermode != kRenderGlow && ent->curstate.rendermode != kRenderTransAdd)
                                qglEnable(GL_FOG);
                        }
                        R_DrawBrushModel(ent);
                        break;

                    case mod_alias:
                        R_DrawAliasModel(ent);
                        break;

                    case mod_studio:
                        if (!ent->curstate.renderamt)
                            break;

                        R_DrawStudioModel(ent, [](int index) { return (*p_transObjects)[index].pEnt; } , *p_numTransObjs);
                        break;

                    default:
                        break;
                }
            }
        }
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
    OPTICK_EVENT();

    if (r_drawentities->value == 0.0)
        return;

    for (int i = 0; i < *p_cl_numvisedicts; i++)
    {
        cl_entity_t* ent = p_cl_visedicts[i];
        *p_currententity = ent;

        if (ent->curstate.rendermode)
        {
            AddTEntity(ent);
            continue;
        }

        switch (ent->model->type)
        {
            case mod_brush:
                R_DrawBrushModel(ent);
                break;

            case mod_studio:
                R_DrawStudioModel(ent, [](int index) { return p_cl_visedicts[index]; } , *p_cl_numvisedicts);
                break;

            default:
                break;
        }
    }

    *p_r_blend = 1.0;

    for (int i = 0; i < *p_cl_numvisedicts; i++)
    {
        cl_entity_t* ent = p_cl_visedicts[i];
        *p_currententity = ent;

        if (ent->curstate.rendermode)
            continue;

        if (ent->model->type == mod_sprite)
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
    OPTICK_EVENT();

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
    OPTICK_EVENT();

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

    *p_mirror = false;

    // Narrowed before anything is drawn and left alone afterwards: the screen rectangle is
    // recomputed from scratch every frame, so this holds for exactly the frame it was asked
    // for. Everything downstream - the viewport, the aspect and the vertical fov - is
    // derived from this rectangle by the engine, so the view is not stretched, it is
    // smaller.
    int inset = render::MaterialEdit_ViewportInset(r_refdef->vrect.width);
    render::MaterialEdit_SetAppliedInset(inset);

    if (inset > 0)
    {
        r_refdef->vrect.width -= inset;

        // Nothing draws over the strip the world no longer reaches, and the panel does not
        // cover all of it, so without this it keeps whatever was last drawn there.
        qglClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        qglClear(GL_COLOR_BUFFER_BIT);
    }

    R_Clear();
    if (!r_refdef->onlyClientDraws)
        R_PreDrawViewModel();

    R_RenderScene();

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
}

void R_ForceCVars(qboolean mp)
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
