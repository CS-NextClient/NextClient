#pragma once

#include <optional>

#include "performance/PerformanceMonitor.h"

#include "graphics/render/GpuBuffer.h"
#include "graphics/render/StudioMaterialRuntime.h"
#include "graphics/render/StudioQueue.h"

namespace performance
{
    class PerformanceMapper;
}

namespace render
{
    void Studio_ApplyFaceCulling(bool flipped);

    struct StudioSubmissionCallbacks
    {
        void (*bind_texture)(void*, unsigned int){};
        void (*note_program_bind)(void*){};
        void* user{};
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
        performance::PerformanceMonitor& monitor_;
        performance::PerformanceMapper& mapper_;
        unsigned int& studio_depth_;
        GpuBuffer bones_;
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
            unsigned int& studio_depth
        );
        bool EnsureBones();
        void Release();
        void Begin() override;
        void Draw(const StudioDrawPacket& packet) override;
        void End() override;

    private:
        void ResetStateCache();
        void ApplyRasterState(const StudioDrawPacket& packet);
        void ApplyBlend(bool enabled, int src_rgb, int dst_rgb, int src_alpha, int dst_alpha);
        void ApplyDepthWrite(bool enabled);
        void ApplyAlphaTest(bool enabled);
    };

    void Studio_CaptureRasterState(StudioDrawPacket& packet, performance::PerformanceMonitor& monitor);
} // namespace render
