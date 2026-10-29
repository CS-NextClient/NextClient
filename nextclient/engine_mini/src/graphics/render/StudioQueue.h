#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <ncl_material/MaterialDoc.h>
#include <ncl_math/vec3.h>
#include <goldsrc/format/studio.h>

#include "graphics/render/StudioIntercept.h"
#include "graphics/render/material/StudioMeshImages.h"

namespace ncl_gl
{
    struct Program;
}

namespace render
{
    struct StudioMaterialBindings;

    inline constexpr size_t kMaxStudioPackets = 128;
    inline constexpr size_t kStudioBoneFloats = goldsrc::MAXSTUDIOBONES * 12;


    enum class StudioFlushReason
    {
        Brush,
        Fallback,
        Client,
        StageEnd,
        Capacity,
        Resource,
        Immediate,
        MutableSkin,
        Count
    };

    enum class StudioImmediateReason
    {
        OutsideOpaque,
        QueueDisabled,
        Callback,
        MutableSkin,
        Count
    };

    struct StudioQueueCallbacks
    {
        void* user{};
        void (*note_flush)(void*, StudioFlushReason, size_t){};
        void (*note_immediate)(void*, StudioImmediateReason){};
    };

    struct StudioQueuedMesh
    {
        const ncl_gl::Program* program{};
        // The runtime owns this generation-scoped record until queued and captured work is discarded.
        const StudioMaterialBindings* material_bindings{};
        std::optional<ncl_material::Material> material;
        unsigned int texture{};
        StudioMeshImages images;
        int skin{};
        float texel_scale[2]{};
        bool masked{};
        bool chrome{};
        bool additive{};
        int shade_mode{};
        int index_count{};
        uintptr_t index_offset{};
    };

    struct StudioRasterState
    {
        bool blend{};
        int blend_src_rgb{};
        int blend_dst_rgb{};
        int blend_src_alpha{};
        int blend_dst_alpha{};
        int blend_equation_rgb{};
        int blend_equation_alpha{};
        bool cull{};
        int cull_face{};
        int front_face{};
        bool depth_test{};
        bool depth_write{};
        int depth_func{};
        double depth_range[2]{};
    };

    struct StudioFogState
    {
        int mode{};
        float color[4]{};
        float density{};
        float start{};
        float end{};
    };

    struct StudioDrawPacket
    {
        std::array<float, kStudioBoneFloats> bones{};
        int bone_count = goldsrc::MAXSTUDIOBONES;
        std::array<float, 16> modelview{};
        std::array<float, 16> projection{};
        std::array<int, 4> viewport{};
        float effect_time{};
        uint32_t texture_revision{};
        StudioRasterState raster;
        StudioFogState fog;
        ncl_math::Vector3 chrome_origin;
        ncl_math::Vector3 view_right;
        unsigned int vao{};
        StudioLighting lighting{};
        bool flipped{};
        float blend{};
        float glow_scale{};
        float bone_scale = 1.0f;
        ncl_math::Vector3 shell_color;
        int light_count{};
        float lights[3][4]{};
        float light_colors[3][3]{};
        float gamma[2]{};
        std::vector<StudioQueuedMesh> meshes;
    };

    struct StudioQueueStats
    {
        uint64_t packets{};
        uint64_t program_binds{};
        size_t max_run{};
        std::array<uint64_t, static_cast<size_t>(StudioFlushReason::Count)> flushes{};
    };

    class StudioSubmissionSink
    {
    public:
        virtual ~StudioSubmissionSink() = default;
        virtual void Begin() = 0;
        virtual void Draw(const StudioDrawPacket& packet) = 0;
        virtual void End() = 0;
    };

    class StudioMeshStorage
    {
    private:
        struct Retained
        {
            std::vector<StudioQueuedMesh> meshes;
            size_t bytes{};
        };

        size_t budget_;
        size_t retained_bytes_{};
        std::vector<Retained> retained_;

    public:
        explicit StudioMeshStorage(size_t budget) :
            budget_(budget)
        {}
        std::vector<StudioQueuedMesh> Acquire();
        void Recycle(std::vector<StudioQueuedMesh>& meshes);
        void Clear();
        size_t get_retained_bytes() const
        {
            return retained_bytes_;
        }
        static bool Measure(const std::vector<StudioQueuedMesh>& meshes, size_t limit, size_t& bytes);
    };

    class StudioDrawQueue
    {
    private:
        friend class StudioRendererTestAccess;
        StudioSubmissionSink& sink_;
        StudioQueueCallbacks callbacks_;
        std::vector<StudioDrawPacket> packets_;
        StudioMeshStorage mesh_storage_{1024 * 1024};
        StudioQueueStats stats_;
        bool deferred_{};
        bool opaque_{};
        bool queue_enabled_{};

    public:
        explicit StudioDrawQueue(StudioSubmissionSink& sink, StudioQueueCallbacks callbacks = {});
        void BeginOpaquePass(bool enabled);
        void EndOpaquePass();
        std::vector<StudioQueuedMesh> AcquireMeshStorage();
        void Submit(const StudioDrawPacket& packet, bool allow_deferred = true);
        void Submit(StudioDrawPacket&& packet, bool allow_deferred = true);
        void Flush(StudioFlushReason reason);
        bool Pause();
        void Resume(bool deferred);
        void Reset();
        void NoteProgramBind();
        const StudioQueueStats& stats() const;
    };
} // namespace render
