#include "graphics/render/StudioIntercept.h"

#include <cstdint>

#include <glad/glad.h>
#include <ncl_studio/studio_mesh_builder.h>
#include <goldsrc/studio_api.h>
#include <goldsrc/format/studio.h>
#include <goldsrc/triangle_api.h>

#include "engine.h"
#include "common/cmd.h"
#include "common/sys_dll.h"
#include "console/console.h"

#include "graphics/render/StudioRenderer.h"
#include "graphics/render/RendererState.h"
#include "graphics/render/effects/studio_effects.h"
#include "graphics/render/model/studio_sub_model.h"
#include "graphics/render/StudioSubmit.h"
#include "performance/performance_scopes.h"
#include "performance/PerformanceMonitor.h"

namespace render
{
    namespace
    {
        constexpr const char* kFlushReasonNames[] =
            {"brush", "fallback", "client", "stage_end", "capacity", "resource", "immediate", "remap"};
        static_assert(std::size(kFlushReasonNames) == static_cast<size_t>(StudioFlushReason::Count));

        template <class T>
        void RestoreEntry(T& entry, T intercept, T original)
        {
            if (entry == intercept)
            {
                entry = original;
            }
        }

        // Mod_LoadStudioModel keeps only the part of the file that precedes the texture
        // pixels when the textures are embedded, so the cached buffer is shorter than the
        // length the header declares. Geometry lives before that point either way.
        size_t CachedModelSize(const goldsrc::studiohdr_t* header)
        {
            return header->textureindex != 0 ? static_cast<size_t>(header->texturedataindex) : static_cast<size_t>(header->length);
        }

        void* ModExtradata(goldsrc::model_t* model)
        {
            return GetRendererState().intercept.ModExtradata(model);
        }
        void StudioSetHeader(void* header)
        {
            GetRendererState().intercept.StudioSetHeader(header);
        }
        void SetRenderModel(goldsrc::model_t* model)
        {
            GetRendererState().intercept.SetRenderModel(model);
        }
        void StudioSetupLighting(goldsrc::alight_t* plight)
        {
            GetRendererState().intercept.StudioSetupLighting(plight);
        }
        void StudioSetupModel(int bodypart, void** ppbodypart, void** ppsubmodel)
        {
            GetRendererState().intercept.StudioSetupModel(bodypart, ppbodypart, ppsubmodel);
        }
        void StudioDrawPoints()
        {
            GetRendererState().intercept.StudioDrawPoints();
        }
        void GL_StudioDrawShadow()
        {
            GetRendererState().intercept.GL_StudioDrawShadow();
        }
        void RestoreRenderer()
        {
            GetRendererState().intercept.RestoreRenderer();
        }
        void StudioDrawBones()
        {
            GetRendererState().intercept.StudioDrawBones();
        }
        void StudioDrawHulls()
        {
            GetRendererState().intercept.StudioDrawHulls();
        }
        void StudioDrawAbsBBox()
        {
            GetRendererState().intercept.StudioDrawAbsBBox();
        }
        void StudioRenderShadow(int count, float* p1, float* p2, float* p3, float* p4)
        {
            GetRendererState().intercept.StudioRenderShadow(count, p1, p2, p3, p4);
        }
        void TriangleBegin(int primitive)
        {
            GetRendererState().intercept.TriangleBegin(primitive);
        }
        void StudioInfo_f()
        {
            GetRendererState().intercept.StudioInfo_f();
        }
    } // namespace

    void* StudioInterceptState::ModExtradata(goldsrc::model_t* model)
    {
        void* extradata = original_.Mod_Extradata(model);

        if (!pass_through_)
        {
            extradata_model_ = model;
            extradata_result_ = extradata;
        }

        return extradata;
    }

    void StudioInterceptState::StudioSetHeader(void* header)
    {
        if (pass_through_)
        {
            original_.StudioSetHeader(header);
            return;
        }

        renderer_.EndModel();
        renderer_.OnModelBoundary();
        header_ = static_cast<goldsrc::studiohdr_t*>(header);
        model_draw_serial_++;

        // Adopted rather than trusted: a header the client did not just look up leaves the
        // model as SetRenderModel last set it, and the draw path checks the pair anyway.
        if (header != nullptr && header == extradata_result_)
        {
            model_ = extradata_model_;
        }

        original_.StudioSetHeader(header);
    }

    void StudioInterceptState::SetRenderModel(goldsrc::model_t* model)
    {
        if (pass_through_)
        {
            original_.SetRenderModel(model);
            return;
        }

        model_ = model;
        original_.SetRenderModel(model);
    }

    void StudioInterceptState::StudioSetupLighting(goldsrc::alight_t* plight)
    {
        original_.StudioSetupLighting(plight);

        if (pass_through_)
        {
            return;
        }

        lighting_.valid = plight != nullptr && plight->plightvec != nullptr;

        if (!lighting_.valid)
        {
            return;
        }

        lighting_.ambient = static_cast<float>(plight->ambientlight);
        lighting_.shade = static_cast<float>(plight->shadelight);

        for (int i = 0; i < 3; i++)
        {
            lighting_.color[i] = plight->color[i];
            lighting_.light_vec[i] = plight->plightvec[i];
        }
    }

    void StudioInterceptState::StudioSetupModel(int bodypart, void** ppbodypart, void** ppsubmodel)
    {
        original_.StudioSetupModel(bodypart, ppbodypart, ppsubmodel);

        if (pass_through_)
        {
            return;
        }

        // Passed on exactly as written: the engine hands out the address of its own
        // submodel pointer, not the pointer, and the draw path decides which it is.
        submodel_ = ppsubmodel != nullptr ? *ppsubmodel : nullptr;
    }

    void StudioInterceptState::StudioDrawPoints()
    {
        if (pass_through_)
        {
            original_.StudioDrawPoints();
            return;
        }

        performance::PerformanceMonitor* monitor = monitor_.get_counter_monitor();
        StudioCpuScope submission(monitor_, studio_depth_);

        draw_points_calls_++;

        shader_drew_last_ = renderer_.TryDraw(CaptureInputs(model_, header_, submodel_));

        if (shader_drew_last_)
        {
            if (monitor)
            {
                monitor->AddCounter(performance::PerfCounterId::StudioShaderSubmissions, 1);
            }

            shader_draw_calls_++;
        }
        else
        {
            renderer_.Flush(StudioFlushReason::Fallback);
            performance::LocalCpuScope stock(monitor_.get_scope_monitor(), performance::PerfStage::StudioStock);

            if (monitor)
            {
                monitor->AddCounter(performance::PerfCounterId::StudioEngineSubmissions, 1);
            }

            original_.StudioDrawPoints();
        }
    }

    void StudioInterceptState::GL_StudioDrawShadow()
    {
        if (pass_through_)
        {
            original_.GL_StudioDrawShadow();
            return;
        }

        if (shader_drew_last_)
        {
            // The engine's own shadow stage opens by restoring the depth mask, and it is the
            // only place in the studio loop that does; skipping the stage without this leaves
            // a translucent entity's disabled mask on for everything drawn after it.
            glDepthMask(GL_TRUE);
            return;
        }

        renderer_.Flush(StudioFlushReason::Fallback);
        original_.GL_StudioDrawShadow();
    }

    void StudioInterceptState::RestoreRenderer()
    {
        if (!pass_through_)
        {
            renderer_.EndModel();
        }

        original_.RestoreRenderer();
    }

    void StudioInterceptState::StudioDrawBones()
    {
        if (!pass_through_)
        {
            renderer_.Flush(StudioFlushReason::Client);
        }

        original_.StudioDrawBones();
    }

    void StudioInterceptState::StudioDrawHulls()
    {
        if (!pass_through_)
        {
            renderer_.Flush(StudioFlushReason::Client);
        }

        original_.StudioDrawHulls();
    }

    void StudioInterceptState::StudioDrawAbsBBox()
    {
        if (!pass_through_)
        {
            renderer_.Flush(StudioFlushReason::Client);
        }

        original_.StudioDrawAbsBBox();
    }

    void StudioInterceptState::StudioRenderShadow(int count, float* p1, float* p2, float* p3, float* p4)
    {
        if (!pass_through_)
        {
            renderer_.Flush(StudioFlushReason::Client);
        }

        original_.StudioRenderShadow(count, p1, p2, p3, p4);
    }

    void StudioInterceptState::TriangleBegin(int primitive)
    {
        if (!pass_through_)
        {
            renderer_.Flush(StudioFlushReason::Client);
        }

        original_triangle_begin_(primitive);
    }

    void StudioInterceptState::StudioInfo_f()
    {
        if (header_ == nullptr)
        {
            Con_Printf("r_studio_info: no studio model has been drawn yet\n");
            return;
        }

        ncl_utils::ResultT<ncl_studio::StudioMesh> mesh = ncl_studio::BuildStudioMesh(header_, CachedModelSize(header_));

        if (mesh.has_error())
        {
            Con_Printf("r_studio_info: %s: %s\n", header_->name, mesh.error_str().c_str());
            return;
        }

        Con_Printf("r_studio_info: %s (%s)\n", header_->name, model_ != nullptr ? model_->name : "<no model>");
        Con_Printf(
            "  %u body parts, %u submodels, %u meshes, %u vertices, %u indices\n",
            static_cast<unsigned>(mesh->bodyparts.size()),
            static_cast<unsigned>(mesh->submodels.size()),
            static_cast<unsigned>(mesh->meshes.size()),
            static_cast<unsigned>(mesh->vertices.size()),
            static_cast<unsigned>(mesh->indices.size())
        );
        Con_Printf("  %u submission calls observed, %u of them drawn by the shader path\n", draw_points_calls_, shader_draw_calls_);
        Con_Printf("  last fallback reason: %s\n", renderer_.get_last_rejection());
        const StudioQueueStats& stats = renderer_.get_queue_stats();
        Con_Printf(
            "  queue: %llu packets, max run %u, %llu studio program binds\n",
            static_cast<unsigned long long>(stats.packets),
            static_cast<unsigned>(stats.max_run),
            static_cast<unsigned long long>(stats.program_binds)
        );

        for (size_t i = 0; i < stats.flushes.size(); i++)
        {
            Con_Printf("    %s: %llu\n", kFlushReasonNames[i], static_cast<unsigned long long>(stats.flushes[i]));
        }
    }

    void StudioInterceptState::PatchApi(
        goldsrc::engine_studio_api_t* api,
        goldsrc::triangleapi_t* triangles,
        const StudioInterceptCallbacks& callbacks
    )
    {
        // The table is handed out again on a level change; comparing against our own entry
        // point both detects that case and keeps a second patch from capturing itself as
        // the original.
        if (api == nullptr || api->StudioDrawPoints == callbacks.studio.StudioDrawPoints)
        {
            return;
        }

        original_ = *api;
        installed_ = callbacks;
        patched_ = api;
        pass_through_ = false;
        renderer_.DiscardPending();

        api->Mod_Extradata = callbacks.studio.Mod_Extradata;
        api->StudioSetHeader = callbacks.studio.StudioSetHeader;
        api->SetRenderModel = callbacks.studio.SetRenderModel;
        api->StudioSetupModel = callbacks.studio.StudioSetupModel;
        api->StudioSetupLighting = callbacks.studio.StudioSetupLighting;
        api->StudioDrawPoints = callbacks.studio.StudioDrawPoints;
        api->GL_StudioDrawShadow = callbacks.studio.GL_StudioDrawShadow;
        api->RestoreRenderer = callbacks.studio.RestoreRenderer;
        api->StudioDrawBones = callbacks.studio.StudioDrawBones;
        api->StudioDrawHulls = callbacks.studio.StudioDrawHulls;
        api->StudioDrawAbsBBox = callbacks.studio.StudioDrawAbsBBox;
        api->StudioRenderShadow = callbacks.studio.StudioRenderShadow;

        if (triangles != nullptr && triangles->Begin != callbacks.triangle_begin)
        {
            patched_triangles_ = triangles;
            original_triangle_begin_ = patched_triangles_->Begin;
            patched_triangles_->Begin = callbacks.triangle_begin;
        }
    }

    const StudioLighting& StudioInterceptState::get_lighting() const
    {
        return lighting_;
    }

    uint32_t StudioInterceptState::get_model_draw_serial() const
    {
        return model_draw_serial_;
    }

    const goldsrc::engine_studio_api_t* StudioInterceptState::get_original_api() const
    {
        return original_.StudioDrawPoints != nullptr ? &original_ : nullptr;
    }

    void StudioInterceptState::Shutdown()
    {
        // Both halves are needed and neither is sufficient. Restoring the table serves whoever
        // reads it after this point; the flag serves everyone already holding a copy of it,
        // which is what the client took at HUD_GetStudioModelInterface time.
        pass_through_ = true;
        renderer_.DiscardPending();

        if (patched_ != nullptr)
        {
            RestoreEntry(patched_->Mod_Extradata, installed_.studio.Mod_Extradata, original_.Mod_Extradata);
            RestoreEntry(patched_->StudioSetHeader, installed_.studio.StudioSetHeader, original_.StudioSetHeader);
            RestoreEntry(patched_->SetRenderModel, installed_.studio.SetRenderModel, original_.SetRenderModel);
            RestoreEntry(patched_->StudioSetupModel, installed_.studio.StudioSetupModel, original_.StudioSetupModel);
            RestoreEntry(patched_->StudioSetupLighting, installed_.studio.StudioSetupLighting, original_.StudioSetupLighting);
            RestoreEntry(patched_->StudioDrawPoints, installed_.studio.StudioDrawPoints, original_.StudioDrawPoints);
            RestoreEntry(patched_->GL_StudioDrawShadow, installed_.studio.GL_StudioDrawShadow, original_.GL_StudioDrawShadow);
            RestoreEntry(patched_->RestoreRenderer, installed_.studio.RestoreRenderer, original_.RestoreRenderer);
            RestoreEntry(patched_->StudioDrawBones, installed_.studio.StudioDrawBones, original_.StudioDrawBones);
            RestoreEntry(patched_->StudioDrawHulls, installed_.studio.StudioDrawHulls, original_.StudioDrawHulls);
            RestoreEntry(patched_->StudioDrawAbsBBox, installed_.studio.StudioDrawAbsBBox, original_.StudioDrawAbsBBox);
            RestoreEntry(patched_->StudioRenderShadow, installed_.studio.StudioRenderShadow, original_.StudioRenderShadow);
        }

        if (patched_triangles_ != nullptr && patched_triangles_->Begin == installed_.triangle_begin)
        {
            patched_triangles_->Begin = original_triangle_begin_;
        }

        patched_triangles_ = nullptr;

        patched_ = nullptr;

        StudioInterceptState::ForgetModel();
    }

    void StudioInterceptState::ForgetModel()
    {
        renderer_.DiscardPending();
        model_draw_serial_++;
        model_ = nullptr;
        header_ = nullptr;
        submodel_ = nullptr;
        extradata_model_ = nullptr;
        extradata_result_ = nullptr;
        lighting_ = StudioLighting{};
        shader_drew_last_ = false;
    }

    void StudioInterceptState::ForgetModel(const goldsrc::model_t* model)
    {
        renderer_.ForgetDrawModel(model);

        if (model_ == model)
        {
            model_draw_serial_++;
            model_ = nullptr;
            header_ = nullptr;
            submodel_ = nullptr;
            lighting_ = StudioLighting{};
            shader_drew_last_ = false;
        }

        if (extradata_model_ == model)
        {
            extradata_model_ = nullptr;
            extradata_result_ = nullptr;
        }
    }

    StudioInterceptState::StudioInterceptState(
        StudioRenderer& renderer,
        StudioEngineBindings& engine,
        performance::PerformanceMonitor& monitor,
        unsigned int& studio_depth
    ) :
        monitor_(monitor),
        studio_depth_(studio_depth),
        renderer_(renderer),
        engine_(engine)
    {}

    StudioModelInputs StudioInterceptState::CaptureInputs(goldsrc::model_t* model, const goldsrc::studiohdr_t* header, void* submodel) const
    {
        const uint32_t serial = model_draw_serial_;
        const StudioLighting lighting = lighting_;
        const goldsrc::engine_studio_api_t* api = get_original_api();
        const goldsrc::cl_entity_t* entity = api != nullptr && api->GetCurrentEntity != nullptr ? api->GetCurrentEntity() : nullptr;
        return {api, model, header, submodel, entity, lighting, serial, model_draw_serial_};
    }

    void Studio_PatchApi(goldsrc::engine_studio_api_t* api)
    {
        Studio_BindEngine();
        StudioInterceptCallbacks callbacks;
        callbacks.studio.Mod_Extradata = ModExtradata;
        callbacks.studio.StudioSetHeader = StudioSetHeader;
        callbacks.studio.SetRenderModel = SetRenderModel;
        callbacks.studio.StudioSetupLighting = StudioSetupLighting;
        callbacks.studio.StudioSetupModel = StudioSetupModel;
        callbacks.studio.StudioDrawPoints = StudioDrawPoints;
        callbacks.studio.GL_StudioDrawShadow = GL_StudioDrawShadow;
        callbacks.studio.RestoreRenderer = RestoreRenderer;
        callbacks.studio.StudioDrawBones = StudioDrawBones;
        callbacks.studio.StudioDrawHulls = StudioDrawHulls;
        callbacks.studio.StudioDrawAbsBBox = StudioDrawAbsBBox;
        callbacks.studio.StudioRenderShadow = StudioRenderShadow;
        callbacks.triangle_begin = TriangleBegin;
        GetRendererState().intercept.PatchApi(api, gEngfuncs.pTriAPI, callbacks);
    }
    const StudioLighting& Studio_Lighting()
    {
        return GetRendererState().intercept.get_lighting();
    }
    uint32_t Studio_ModelDrawSerial()
    {
        return GetRendererState().intercept.get_model_draw_serial();
    }
    const goldsrc::engine_studio_api_t* Studio_OriginalApi()
    {
        return GetRendererState().intercept.get_original_api();
    }
    void Studio_Shutdown()
    {
        GetRendererState().intercept.Shutdown();
    }
    void Studio_ForgetModel()
    {
        GetRendererState().intercept.ForgetModel();
    }
    void Studio_ForgetModel(const goldsrc::model_t* model)
    {
        GetRendererState().intercept.ForgetModel(model);
    }
    void Studio_RegisterCommands()
    {
        Cmd_AddCommand("r_studio_info", StudioInfo_f);
    }
} // namespace render
