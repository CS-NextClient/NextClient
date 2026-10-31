#pragma once

#include <optional>

#include "graphics/render/GpuBuffer.h"
#include "graphics/render/StudioMaterialRuntime.h"
#include "graphics/render/StudioQueue.h"
#include "performance/PerformanceMonitor.h"

namespace performance
{
    class PerformanceMapper;
}

namespace render
{
    class StudioPhaseCapture;
    void Studio_ApplyFaceCulling(bool flipped);

    struct StudioSubmissionCallbacks
    {
        void (*bind_texture)(void*, unsigned int){};
        void (*note_program_bind)(void*){};
        void* user{};
    };

    enum class StudioRigidFallback
    {
        PrimitiveRestart,
        Ineligible,
        SharedState,
        Singleton,
        Resource,
        Count
    };

    struct StudioSubmissionStats
    {
        uint64_t physical_draws{};
        uint64_t logical_groups{};
        uint64_t logical_triangles{};
        uint64_t submitted_triangles{};
        uint64_t instance_batches{};
        uint64_t saved_calls{};
        uint64_t instance_bytes{};
        uint64_t instance_uploads{};
        std::array<uint64_t, static_cast<size_t>(StudioRigidFallback::Count)> fallbacks{};
    };

    class StudioCpuScope
    {
    private:
        performance::PerformanceMonitor& monitor_;
        unsigned int& depth_;
        ncl_perf::PerfScopeToken_t token_{};

    public:
        explicit StudioCpuScope(
            performance::PerformanceMonitor& monitor,
            unsigned int& depth,
            std::source_location location = std::source_location::current()
        );
        ~StudioCpuScope();

        StudioCpuScope(const StudioCpuScope&) = delete;
        StudioCpuScope& operator=(const StudioCpuScope&) = delete;
    };

    class StudioGlSubmission : public StudioSubmissionSink
    {
    private:
        struct InstanceRecord
        {
            float rows[12]{};
            uint32_t first_vertex{};
            uint32_t end_vertex{};
            uint32_t blend_bits{};
            uint32_t padding{};
        };

        performance::PerformanceMonitor& monitor_;
        performance::PerformanceMapper& mapper_;
        unsigned int& studio_depth_;
        StudioPhaseCapture* phase_capture_{};
        GpuBuffer bones_;
        GpuBuffer instances_;
        std::array<InstanceRecord, kMaxStudioPackets> instance_records_{};
        StudioSubmissionStats stats_;
        StudioMaterialRuntime& material_runtime_;
        StudioSubmissionCallbacks callbacks_;
        std::optional<StudioCpuScope> cpu_scope_;
        GLint previous_program_{};
        GLint previous_vao_{};
        GLint previous_array_buffer_{};
        GLint previous_uniform_buffer_{};
        GLint previous_bones_{};
        GLint64 previous_bones_start_{};
        GLint64 previous_bones_size_{};
        GLint previous_matrix_mode_{};
        GLint previous_texture_{};
        StudioMaterialState material_state_;
        unsigned int bound_vao_{};
        StudioRasterState applied_raster_;
        bool raster_valid_{};
        std::optional<bool> alpha_test_;
        bool alpha_func_set_{};
        std::optional<unsigned int> bound_texture_;
        std::vector<GLsizei> counts_;
        std::vector<const void*> offsets_;

    public:
        StudioGlSubmission(
            RenderDevice& device,
            StudioMaterialRuntime& material_runtime,
            StudioSubmissionCallbacks callbacks,
            performance::PerformanceMonitor& monitor,
            performance::PerformanceMapper& mapper,
            unsigned int& studio_depth,
            StudioPhaseCapture* phase_capture = nullptr
        );
        bool EnsureBones();
        bool EnsureInstances();
        const StudioSubmissionStats& get_stats() const
        {
            return stats_;
        }
        void ResetStats();
        void Release();
        void Begin() override;
        void Draw(const StudioDrawPacket& packet) override;
        void DrawRun(std::span<const StudioDrawPacket> packets) override;
        void End() override;

    private:
        void ResetStateCache();
        void DrawPacket(const StudioDrawPacket& packet, size_t instances);
        bool IsRigidEligible(const StudioDrawPacket& packet) const;
        void BindInstances(const StudioDrawPacket& packet, size_t first);
        void ApplyRasterState(const StudioDrawPacket& packet);
        void ApplyBlend(bool enabled, int src_rgb, int dst_rgb, int src_alpha, int dst_alpha);
        void ApplyDepthWrite(bool enabled);
        void ApplyAlphaTest(bool enabled);
    };

    void Studio_CaptureRasterState(StudioDrawPacket& packet, performance::PerformanceMonitor& monitor);
} // namespace render
