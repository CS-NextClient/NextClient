#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <goldsrc/format/studio.h>
#include <goldsrc/fwd.h>
#include <ncl_math/vec3.h>

namespace performance
{
    class PerformanceMonitor;
}

namespace ncl_studio
{
    struct StudioMesh;
}

namespace render
{
    class RenderDevice;

    struct StudioBoneBounds
    {
        int bone{};
        ncl_math::Vector3 minimum{};
        ncl_math::Vector3 maximum{};
    };

    struct StudioRigidRange
    {
        uint32_t first_vertex{};
        uint32_t end_vertex{};
        uint32_t first_index{};
        uint32_t index_count{};
        uint32_t bone{};
    };

    inline constexpr size_t kStudioRigidMaxRanges = 3;
    inline constexpr uint32_t kStudioRigidMaxIndices = 180;

    struct StudioRigidCatalog
    {
        std::array<StudioRigidRange, kStudioRigidMaxRanges> ranges{};
        uint32_t count{};
        uint32_t index_count{};
    };

    // One studio model uploaded to the GPU: the whole model shares a single vertex and
    // index buffer, and every mesh is drawn as a range inside them.
    struct StudioGpuModel
    {
        // GLuint, spelled out so this header does not drag in a GL loader; engine_mini
        // translation units disagree on which GL header comes first.
        unsigned int vao;
        const ncl_studio::StudioMesh* mesh;
        std::vector<StudioBoneBounds> bounds;
        unsigned int rigid_vao{};
        StudioRigidCatalog rigid;
    };

    class StudioGeometry
    {
    private:
        struct State;
        RenderDevice& device_;
        performance::PerformanceMonitor& monitor_;
        std::unique_ptr<State> state_;

    public:
        explicit StudioGeometry(RenderDevice& device, performance::PerformanceMonitor& monitor);
        ~StudioGeometry();
        const StudioGpuModel* Prepare(const std::string& name, const goldsrc::studiohdr_t* header, size_t size);
        void Release();
        void OnDeviceLost();

    private:
        bool Upload();
        void PrepareRigidVao();
    };
} // namespace render
