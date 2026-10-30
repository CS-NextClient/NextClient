#include "graphics/render/StudioDraw.h"

#include <algorithm>
#include <glad/glad.h>

#include "engine.h"
#include "common/cvar.h"
#include "graphics/gl_local.h"
#include "graphics/view.h"
#include "graphics/render/RendererState.h"
#include "graphics/render/StudioRenderer.h"
#include "graphics/render/StudioShaderInterface.h"

namespace render
{
    bool StudioEngineBindings::is_network_session() const
    {
        return client != nullptr && client->state != goldsrc::ca_disconnected;
    }

    StudioViewInputs StudioEngineBindings::CaptureView()
    {
        if (!controls_resolved)
        {
            controls_resolved = true;
            right_hand = Cvar_FindVar("cl_righthand");
            shell_frequency = Cvar_FindVar("r_glowshellfreq");
        }

        StudioViewInputs result;
        result.network_session = is_network_session();
        result.shader_enabled = config != nullptr && config->studio_shader != nullptr && config->studio_shader->value != 0.0f;
        result.materials_enabled = config == nullptr || config->studio_materials == nullptr || config->studio_materials->value != 0.0f;
        result.queue_enabled = config != nullptr && config->studio_queue != nullptr && config->studio_queue->value != 0.0f;
        result.blend = blend != nullptr ? *blend : 1.0f;
        result.gamma = gamma != nullptr && *gamma != nullptr && (*gamma)->value > 0.0f ? (*gamma)->value : 2.5f;
        result.right_hand = right_hand != nullptr ? right_hand->value : 0.0f;
        result.shell_frequency = shell_frequency != nullptr ? shell_frequency->value : 0.0f;
        result.time = state != nullptr ? state->time : 0.0;
        result.view_entity = state != nullptr ? state->viewent.index : 0;
        if (view != nullptr)
        {
            std::copy_n(view->vieworg, 3, result.origin);
            std::copy_n(view->viewangles, 3, result.angles);
        }
        return result;
    }

    std::array<float, kStudioLightGammaEntries> StudioEngineBindings::CaptureLightGamma() const
    {
        std::array<float, kStudioLightGammaEntries> result;
        for (size_t index = 0; index < result.size(); ++index)
        {
            result[index] = static_cast<float>(light_gamma[index * kStudioGammaTableStride]) / 1023.0f;
        }
        return result;
    }

    void StudioEngineBindings::BindCapturedTexture(unsigned int texture)
    {
        GL_SelectTexture(TEXTURE0_SGIS);
        glActiveTexture(GL_TEXTURE0 + kStudioBaseTextureUnit);
        *current_texture = -1;
        GL_Bind(static_cast<int>(texture));
    }

    unsigned int StudioEngineBindings::CaptureSkin(const goldsrc::engine_studio_api_t& api, const goldsrc::studiohdr_t* header, int skin)
    {
        GL_SelectTexture(TEXTURE0_SGIS);
        glActiveTexture(GL_TEXTURE0 + kStudioBaseTextureUnit);
        api.StudioSetupSkin(const_cast<goldsrc::studiohdr_t*>(header), skin);
        GLint texture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        return static_cast<GLuint>(texture);
    }

    void Studio_BindEngine()
    {
        StudioEngineBindings& bindings = GetRendererState().studio_engine;
        bindings.client = cls;
        bindings.state = cl;
        bindings.view = r_refdef;
        bindings.lights = p_cl_elights != nullptr ? *p_cl_elights : nullptr;
        bindings.light_count = kMaxElights;
        bindings.blend = p_r_blend;
        bindings.gamma = &gamma_cvar;
        bindings.config = &g_RenderCfg;
        bindings.current_texture = p_currenttexture;
        bindings.linear_gamma = lineargammatable;
        bindings.light_gamma = lightgammatable;
    }

    StudioQueuePause::StudioQueuePause() :
        StudioQueuePause(GetRendererState().studio)
    {}
    StudioQueuePause::StudioQueuePause(StudioRenderer& renderer) :
        renderer_(renderer),
        cancellation_epoch_(renderer.get_cancellation_epoch())
    {
        renderer_.FlushPending();
        if (cancellation_epoch_ == renderer_.get_cancellation_epoch())
        {
            deferred_ = renderer_.queue_.Pause();
        }
    }

    StudioQueuePause::~StudioQueuePause()
    {
        if (cancellation_epoch_ == renderer_.get_cancellation_epoch())
        {
            renderer_.FlushPending();
            if (cancellation_epoch_ == renderer_.get_cancellation_epoch())
            {
                renderer_.queue_.Resume(deferred_);
            }
        }
    }

    StudioPlayerEffectsScope::StudioPlayerEffectsScope(const goldsrc::cl_entity_t* entity) :
        StudioPlayerEffectsScope(GetRendererState().studio, Studio_OriginalApi(), entity)
    {}

    StudioPlayerEffectsScope::StudioPlayerEffectsScope(
        StudioRenderer& renderer,
        const goldsrc::engine_studio_api_t* api,
        const goldsrc::cl_entity_t* entity
    ) :
        renderer_(renderer),
        previous_entity_(renderer.effect_player_),
        previous_effects_(renderer.attachment_effects_)
    {
        cancellation_epoch_ = renderer_.get_cancellation_epoch();
        renderer_.effect_player_ = entity;
        renderer_.attachment_effects_ = false;
        const StudioViewInputs view = renderer_.engine_.CaptureView();
        if (view.shader_enabled && view.materials_enabled && api != nullptr && api->Mod_Extradata != nullptr && entity != nullptr &&
            renderer_.device_.CheckAvailable())
        {
            renderer_.attachment_effects_ = renderer_.CheckAttachmentEffects(api, entity, entity->model);
        }
    }

    StudioPlayerEffectsScope::~StudioPlayerEffectsScope()
    {
        if (cancellation_epoch_ == renderer_.get_cancellation_epoch())
        {
            renderer_.effect_player_ = previous_entity_;
            renderer_.attachment_effects_ = previous_effects_;
        }
    }

    bool Studio_TryDraw(goldsrc::model_t* model, const goldsrc::studiohdr_t* header, void* submodel)
    {
        Studio_BindEngine();
        StudioInterceptState& intercept = GetRendererState().intercept;
        return GetRendererState().studio.TryDraw(intercept.CaptureInputs(model, header, submodel));
    }
    void Studio_BeginOpaquePass()
    {
        Studio_BindEngine();
        RendererState& state = GetRendererState();
        state.studio.BeginOpaquePass(state.studio_engine.CaptureView().queue_enabled);
    }
    void Studio_EndOpaquePass()
    {
        GetRendererState().studio.EndOpaquePass();
    }
    void Studio_BeginTransparentPass()
    {
        Studio_BindEngine();
        RendererState& state = GetRendererState();
        state.studio.BeginTransparentPass(state.studio_engine.CaptureView().queue_enabled);
    }
    void Studio_EndTransparentPass()
    {
        GetRendererState().studio.EndTransparentPass();
    }
    void Studio_EndModel()
    {
        GetRendererState().studio.EndModel();
    }
    void Studio_Flush(StudioFlushReason reason)
    {
        GetRendererState().studio.Flush(reason);
    }
    const StudioQueueStats& Studio_QueueStats()
    {
        return GetRendererState().studio.get_queue_stats();
    }
    void Studio_DiscardPending()
    {
        GetRendererState().studio.DiscardPending();
    }
    void Studio_ReleaseContextResources()
    {
        GetRendererState().studio.ReleaseContextResources();
    }
    void Studio_InvalidateContextReferences()
    {
        GetRendererState().studio.InvalidateContextReferences();
    }
    void Studio_ForgetDrawModel(const goldsrc::model_t* model)
    {
        GetRendererState().studio.ForgetDrawModel(model);
    }
    const char* Studio_LastRejection()
    {
        return GetRendererState().studio.get_last_rejection();
    }
    const char* Studio_ShaderDefines()
    {
        return kStudioShadeDefines;
    }
    void Studio_WarmPrograms()
    {
        GetRendererState().studio.WarmPrograms();
    }
    void Studio_OnGammaChanged()
    {
        Studio_BindEngine();
        RendererState& state = GetRendererState();
        state.studio.OnGammaChanged(state.studio_engine.CaptureLightGamma());
    }
} // namespace render
