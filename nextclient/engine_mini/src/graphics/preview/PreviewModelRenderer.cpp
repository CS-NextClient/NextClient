#include "graphics/preview/PreviewModelRenderer.h"

#include <glad/glad.h>
#include <goldsrc/format/studio.h>
#include <ncl_math/vec3.h>

#include "graphics/gl_local.h"
#include "graphics/ViewmodelFrustumCalculator.h"
#include "graphics/preview/preview_model.h"
#include "graphics/preview/preview_view.h"
#include "graphics/preview/PreviewModelRepository.h"
#include "graphics/render/StudioPreviewContext.h"
#include "graphics/render/StudioRenderer.h"

namespace render::preview
{
    namespace
    {
        PreviewModelRenderer* g_CallbackRenderer;

        void PreviewDynamicLight(goldsrc::cl_entity_t* entity, goldsrc::alight_t* light)
        {
            g_CallbackRenderer->DynamicLight(entity, light);
        }

        int PreviewCheckBBox()
        {
            return g_CallbackRenderer->CheckBBox();
        }

        goldsrc::mstudioseqdesc_t* GetSequence(goldsrc::studiohdr_t* header, int sequence)
        {
            if (sequence < 0 || sequence >= header->numseq)
            {
                return nullptr;
            }

            return reinterpret_cast<goldsrc::mstudioseqdesc_t*>(reinterpret_cast<uint8_t*>(header) + header->seqindex) + sequence;
        }

    } // namespace

    PreviewModelRenderer::PreviewModelRenderer(PreviewModelRepository& models, PreviewEngineBindings& engine, StudioRenderer& studio) :
        models_(models),
        engine_(engine),
        studio_(studio)
    {}

    void PreviewModelRenderer::SetupEntity(
        goldsrc::model_t* model,
        int sequence,
        int frame_count,
        float frame,
        float time,
        const float* origin,
        const float* angles
    )
    {
        // Never the view entity's index: with cl_righthand on, the engine takes an entity
        // holding it for its own flipped view model and turns culling and lighting over.
        entity_.index = -1;
        entity_.player = 0;
        entity_.model = model;

        goldsrc::entity_state_t& state = entity_.curstate;

        ncl_math::VectorCopy(origin, entity_.origin);
        ncl_math::VectorCopy(angles, entity_.angles);
        ncl_math::VectorCopy(origin, state.origin);
        ncl_math::VectorCopy(angles, state.angles);

        state.movetype = goldsrc::MOVETYPE_NONE;
        PreviewModel_SetAnimationPose(entity_, sequence, frame_count, frame, time);

        state.scale = 1.0f;
        state.rendermode = goldsrc::kRenderNormal;
        state.renderamt = 255;
        state.body = 0;
        state.skin = 0;

        for (int i = 0; i < 4; i++)
        {
            state.controller[i] = 127;
        }

        state.blending[0] = 0;
        state.blending[1] = 0;


        ncl_math::VectorCopy(origin, entity_.latched.prevorigin);
        ncl_math::VectorCopy(angles, entity_.latched.prevangles);
    }
    void PreviewModelRenderer::PatchStudioApi(goldsrc::engine_studio_api_t* api)
    {
        // The table is the engine's own global and outlives one query of it, so a second patch
        // would take the wrappers below for the engine's functions and recurse for ever.
        if (api->StudioDynamicLight == &PreviewDynamicLight)
        {
            return;
        }

        RestoreStudioApi();
        g_CallbackRenderer = this;
        patched_api_ = api;

        original_dynamic_light_ = api->StudioDynamicLight;
        api->StudioDynamicLight = &PreviewDynamicLight;

        original_check_bbox_ = api->StudioCheckBBox;
        api->StudioCheckBBox = &PreviewCheckBBox;
    }

    void PreviewModelRenderer::RestoreStudioApi()
    {
        if (patched_api_ == nullptr)
        {
            return;
        }

        if (patched_api_->StudioDynamicLight == &PreviewDynamicLight)
        {
            patched_api_->StudioDynamicLight = original_dynamic_light_;
        }

        if (patched_api_->StudioCheckBBox == &PreviewCheckBBox)
        {
            patched_api_->StudioCheckBBox = original_check_bbox_;
        }

        // The client retains copied callbacks after the shared table is restored.
        patched_api_ = nullptr;
    }

    int PreviewModelRenderer::Draw(
        const PreviewModel& model,
        const PreviewDrawParams& params,
        int wide,
        int tall,
        PreviewInteraction* capture
    )
    {
        if (engine_.studio == nullptr || engine_.studio->StudioDrawModel == nullptr || engine_.current_entity == nullptr ||
            engine_.blend == nullptr || engine_.load_model == nullptr || engine_.model_data == nullptr)
        {
            return -1;
        }

        if (wide <= 0 || tall <= 0 || model.path == nullptr || model.path[0] == '\0')
        {
            return -1;
        }

        const uint64_t preparation_epoch = studio_.get_cancellation_epoch();
        PreviewModelInfo info{};
        PreparedPreviewModel prepared_model{};
        PreviewModelStatus prepared = models_.PrepareModel(model.path, &info, &prepared_model, studio_.get_resource_barrier());

        if (preparation_epoch != studio_.get_cancellation_epoch())
        {
            return -1;
        }

        if (prepared != PreviewModelStatus::Ready &&
            !(prepared == PreviewModelStatus::RendererUnavailable && params.mode == PreviewRenderMode::Configured))
        {
            return -1;
        }

        goldsrc::model_t* loaded_model = prepared_model.model;
        goldsrc::studiohdr_t* header = prepared_model.header;

        if (loaded_model == nullptr)
        {
            loaded_model = engine_.load_model(model.path, false, true);
            if (preparation_epoch != studio_.get_cancellation_epoch() || loaded_model == nullptr ||
                loaded_model->type != goldsrc::mod_studio)
            {
                return -1;
            }
            header = static_cast<goldsrc::studiohdr_t*>(engine_.model_data(loaded_model));
        }

        if (preparation_epoch != studio_.get_cancellation_epoch() || header == nullptr)
        {
            return -1;
        }

        render::StudioPreviewContext context(studio_, &entity_, params, model, capture);
        if (context.is_rejected() || loaded_model->type != goldsrc::mod_studio || loaded_model->cache.data != header ||
            engine_.studio == nullptr || engine_.studio->StudioDrawModel == nullptr || engine_.current_entity == nullptr ||
            engine_.blend == nullptr)
        {
            return -1;
        }

        const goldsrc::mstudioseqdesc_t* sequence = GetSequence(header, model.sequence);

        if (sequence == nullptr)
        {
            return -1;
        }

        const int frame_count = sequence->numframes;
        bool mirror = model.camera_space && model.mirror;

        if (model.camera_space)
        {
            PreviewView_LoadCameraSpace(ViewmodelFrustumCalculator::CalcVerticalFov(model.fov), mirror, wide, tall);
        }
        else
        {
            PreviewView_LoadCamera(params.camera, wide, tall);
        }

        // The mirror turns the space over about the lateral axis: the model is placed on the
        // other side of it to stay put, and the angles about the mirrored axes, and the light,
        // change hand with it.
        float origin[3] = {model.origin[0], mirror ? -model.origin[1] : model.origin[1], model.origin[2]};
        float angles[3] = {model.angles[0], mirror ? -model.angles[1] : model.angles[1], mirror ? -model.angles[2] : model.angles[2]};

        const PreviewLight& light = studio_.PreviewInputs(&entity_)->light;

        ambient_light_ = light.model_ambient;
        shade_light_ = light.model_shade;

        ncl_math::VectorCopy(light.color, light_color_);
        ncl_math::VectorCopy(light.direction, light_direction_);

        SetupEntity(loaded_model, model.sequence, frame_count, model.frame, static_cast<float>(params.time), origin, angles);
        entity_.curstate.skin = model.skin;
        entity_.curstate.body = model.body;

        // The engine culls front faces and turns the test back on after every body part, so a
        // mirrored model is drawn with the winding turned over rather than without the test.
        qglEnable(GL_CULL_FACE);
        qglCullFace(GL_FRONT);
        qglFrontFace(mirror ? GL_CW : GL_CCW);

        // Vertex alpha comes from this, and the entity pass that sets it never runs in the menu:
        // it holds whatever the last map left, zero on a fresh start.
        float saved_blend = *engine_.blend;
        *engine_.blend = 1.0f;

        goldsrc::cl_entity_t* saved_current = *engine_.current_entity;
        *engine_.current_entity = &entity_;

        const uint64_t epoch = studio_.get_cancellation_epoch();
        goldsrc::cl_entity_t** current_owner = engine_.current_entity;
        float* blend_owner = engine_.blend;
        int drawn = engine_.studio->StudioDrawModel(goldsrc::STUDIO_RENDER);
        if (epoch != studio_.get_cancellation_epoch() || engine_.current_entity != current_owner || engine_.blend != blend_owner)
        {
            return -1;
        }

        *engine_.current_entity = saved_current;
        *engine_.blend = saved_blend;
        qglFrontFace(GL_CCW);

        return context.is_rejected() && params.mode == PreviewRenderMode::MaterialAuthoring ? -1 : drawn;
    }
    void PreviewModelRenderer::DynamicLight(goldsrc::cl_entity_t* ent, goldsrc::alight_t* plight)
    {
        if (patched_api_ == nullptr || ent != &entity_)
        {
            original_dynamic_light_(ent, plight);
            return;
        }

        plight->ambientlight = ambient_light_;
        plight->shadelight = shade_light_;

        ncl_math::VectorCopy(light_color_, plight->color);
        ncl_math::VectorCopy(light_direction_, plight->plightvec);
    }

    // The engine tests against the frustum of the last frame it drew itself, which after a
    // map is the player's view; a preview is drawn through a view of its own.
    int PreviewModelRenderer::CheckBBox()
    {
        if (patched_api_ != nullptr && *engine_.current_entity == &entity_)
        {
            return TRUE;
        }

        return original_check_bbox_();
    }


} // namespace render::preview
