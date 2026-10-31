#pragma once

#include "graphics/render/StudioDraw.h"
#include "graphics/render/StudioDump.h"
#include "graphics/render/StudioEngineBindings.h"
#include "graphics/render/StudioSubmit.h"
#include "graphics/render/model/StudioModelCache.h"

namespace render
{
    class StudioEffects;
    class StudioDiagnostics;
    class MaterialCoverageRuntime;
    class ServerMaterials;
    class MaterialEditSession;
    struct StudioPreviewInputs;

    struct StudioModelInputs
    {
        const goldsrc::engine_studio_api_t* api{};
        goldsrc::model_t* model{};
        const goldsrc::studiohdr_t* header{};
        void* submodel{};
        const goldsrc::cl_entity_t* entity{};
        StudioLighting lighting{};
        uint32_t serial{};
        const uint32_t& live_serial;
    };

    class StudioRenderer final : public IDeviceResource
    {
    private:
        friend class StudioQueuePause;
        friend class StudioPlayerEffectsScope;
        friend class StudioPreviewContext;
        friend class StudioRendererTestAccess;

        struct ModelDrawCache
        {
            uint32_t serial{};
            const goldsrc::model_t* model{};
            const goldsrc::model_t* texture_model{};
            const goldsrc::studiohdr_t* header{};
            const goldsrc::cl_entity_t* entity{};
            const goldsrc::studiohdr_t* texture_header{};
            bool bones_captured{};
            bool stable_skins{};
            StudioPose bones;
        };

        struct PendingModel : StudioDrawPacket
        {
            const goldsrc::model_t* model{};
            const StudioGpuModel* effect_model{};
            bool include_occluders{};
            uintptr_t effect_identity{};
            bool active{};
            const goldsrc::cl_entity_t* entity{};
            bool allow_deferred{};
            int expected_parts{};
            int seen_parts{};
            size_t mesh_count{};
        };

        struct EntityLights
        {
            int count{};
            float light[3][4]{};
            float color[3][3]{};
        };

        performance::PerformanceMonitor& monitor_;
        unsigned int studio_depth_{};
        RenderDevice& device_;
        ShaderManager& shaders_;
        MaterialTextureCache& textures_;
        MaterialCoverageRuntime& coverage_;
        uint64_t preparation_epoch_ = 1;
        uint64_t cancellation_epoch_ = 1;
        StudioModelCache& models_;
        ServerMaterials& server_materials_;
        const MaterialEditSession& edits_;
        StudioEffects& effects_;
        StudioEngineBindings& engine_;
        StudioDiagnostics& diagnostics_;
        StudioMaterialRuntime material_runtime_;
        StudioGlSubmission submission_sink_;
        StudioPosePool poses_;
        StudioDrawQueue queue_;
        float scaled_bones_[kStudioBoneFloats]{};
        bool light_gamma_ready_{};
        ModelDrawCache model_draw_;
        const goldsrc::cl_entity_t* effect_player_{};
        bool attachment_effects_{};
        uint32_t abandoned_serial_{};
        bool abandoned_{};
        const char* last_rejection_ = "not attempted";
        char rejection_detail_[256]{};
        PendingModel pending_;
        StudioSubmission submission_;
        StudioPreviewInputs* preview_inputs_{};

    public:
        StudioRenderer(
            RenderDevice& device,
            ShaderManager& shaders,
            MaterialManifestCatalog& manifests,
            MaterialTextureCache& textures,
            StudioModelCache& models,
            ServerMaterials& server_materials,
            const MaterialEditSession& edits,
            StudioEffects& effects,
            StudioEngineBindings& engine,
            StudioDiagnostics& diagnostics,
            MaterialCoverageRuntime& coverage,
            performance::PerformanceMonitor& monitor,
            performance::PerformanceMapper& mapper
        );
        ~StudioRenderer() override;
        StudioRenderer(const StudioRenderer&) = delete;
        StudioRenderer& operator=(const StudioRenderer&) = delete;
        bool TryDraw(const StudioModelInputs& inputs);
        void BeginOpaquePass(bool enabled);
        void EndOpaquePass();
        void BeginTransparentPass(bool enabled);
        void EndTransparentPass();
        void EndModel();
        void Flush(StudioFlushReason reason);
        const StudioQueueStats& get_queue_stats() const;
        const StudioSubmissionStats& get_submission_stats() const
        {
            return submission_sink_.get_stats();
        }
        void ResetSubmissionStats()
        {
            submission_sink_.ResetStats();
        }
        void DiscardPending();
        void InvalidateContextReferences();
        void ReleaseContextResources();
        void ForgetDrawModel(const goldsrc::model_t* model);
        const char* get_last_rejection() const;
        void WarmPrograms();
        void OnGammaChanged(const std::array<float, kStudioLightGammaEntries>& light_gamma);
        void OnDeviceLost() override;
        void ClearDiagnostics();
        void OnModelBoundary();
        const uint64_t& get_cancellation_source() const
        {
            return cancellation_epoch_;
        }
        uint64_t get_cancellation_epoch() const
        {
            return cancellation_epoch_;
        }
        bool IsContextCurrent()
        {
            return device_.IsContextCurrent();
        }
        const StudioPreviewInputs* PreviewInputs(const goldsrc::cl_entity_t* entity) const;
        void CapturePreviewPacket(const goldsrc::cl_entity_t* entity, const StudioDrawPacket& packet);
        void RejectPreview(const goldsrc::cl_entity_t* entity);
        StudioModelBarrier get_resource_barrier();
        performance::PerformanceMonitor& get_monitor()
        {
            return monitor_;
        }
        unsigned int& get_studio_depth()
        {
            return studio_depth_;
        }

    private:
        const float* ScaleBones(const float* bones, int count, const float* origin, float scale);
        bool Reject(const char* reason);
        bool RejectSubModel(const goldsrc::studiohdr_t* header, void* submodel, const ncl_studio::StudioMesh& mesh);
        bool CheckAttachmentEffects(
            const goldsrc::engine_studio_api_t* api,
            const goldsrc::cl_entity_t* entity,
            const goldsrc::model_t* parent
        );
        bool IsFlippedViewModel(const goldsrc::cl_entity_t* entity, const StudioViewInputs& view);
        void SelectEntityLights(const goldsrc::cl_entity_t* entity, const StudioViewInputs& view, EntityLights& out);
        ncl_math::Vector2 GammaExponents(const StudioViewInputs& view);
        ncl_math::Vector3 ShellChromeOrigin(const StudioViewInputs& view);
        void FlushPending();
        bool RejectSubmission(const char* reason, size_t queued_before, bool counted);
        bool TryDrawSubmission(const StudioModelInputs& inputs);
        static void NoteQueueFlush(void*, StudioFlushReason reason, size_t packets);
        static void NoteImmediate(void*, StudioImmediateReason reason);
        static void NoteProgramBind(void* user);
        static void BindCapturedTexture(void* user, unsigned int texture);
        static bool MaterialHasEffects(void* user, const ncl_material::Material& material);
    };
} // namespace render
