#include "graphics/render/model/StudioGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include <ncl_studio/StudioMesh.h>
#include <ncl_studio/studio_mesh_builder.h>

#include "console/console.h"
#include "graphics/render/GpuBuffer.h"
#include "graphics/render/RenderDevice.h"
#include "graphics/render/StudioBindings.h"
#include "performance/PerformanceMonitor.h"
#include "performance/performance_scopes.h"

namespace render
{
    namespace
    {
        // Bones are byte indices in the file, so they travel as integers rather than being
        // converted to float and back in the shader. The shell normal travels as signed bytes
        // for the same kind of reason: it is a unit direction that only the glow shell reads,
        // and at full width it would have cost every studio model a third of its buffer for a
        // pass that almost no frame contains.
        struct GpuVertex
        {
            float position[3];
            float normal[3];
            float texel[2];
            uint8_t position_bone;
            uint8_t normal_bone;
            uint8_t shell_normal_bone;
            uint8_t bone_padding;
            int8_t shell_normal[3];
            int8_t padding;
        };

        // Drivers decode a signed normalized byte as value / 127 (the GL 4.2 rule; the 3.3
        // text says (2c+1)/255, which DX10-class hardware never implemented), so 127 is the
        // unit.
        int8_t QuantizeNormal(float value)
        {
            float scaled = value * 127.0f;

            return static_cast<int8_t>(std::clamp(scaled < 0.0f ? scaled - 0.5f : scaled + 0.5f, -127.0f, 127.0f));
        }

        StudioRigidCatalog BuildRigidCatalog(const ncl_studio::StudioMesh& mesh)
        {
            StudioRigidCatalog catalog;

            if (mesh.bodyparts.size() != 1 || mesh.submodels.empty() || mesh.submodels.size() > catalog.ranges.size())
            {
                return {};
            }

            uint32_t end_index = 0;
            for (const ncl_studio::SubModelRange& submodel : mesh.submodels)
            {
                if (submodel.mesh_count != 1)
                {
                    return {};
                }

                const ncl_studio::MeshRange& range = mesh.meshes[submodel.first_mesh];

                if (range.first_index != end_index || range.index_count == 0 || range.index_count % 3 != 0 ||
                    range.index_count > kStudioRigidMaxIndices - end_index ||
                    (catalog.count != 0 && range.index_count != catalog.ranges[0].index_count))
                {
                    return {};
                }

                StudioRigidRange rigid;
                rigid.first_vertex = std::numeric_limits<uint32_t>::max();
                rigid.first_index = range.first_index;
                rigid.index_count = range.index_count;
                rigid.bone = mesh.vertices[mesh.indices[range.first_index]].position_bone;
                end_index += range.index_count;

                for (uint32_t index = range.first_index; index < end_index; ++index)
                {
                    const uint32_t vertex_index = mesh.indices[index];
                    const ncl_studio::Vertex& vertex = mesh.vertices[vertex_index];

                    if (vertex.position_bone != rigid.bone || vertex.normal_bone != rigid.bone)
                    {
                        return {};
                    }

                    for (int axis = 0; axis < 3; ++axis)
                    {
                        if (!std::isfinite(vertex.position[axis]) || !std::isfinite(vertex.normal[axis]))
                        {
                            return {};
                        }
                    }

                    if (!std::isfinite(vertex.texel.x) || !std::isfinite(vertex.texel.y))
                    {
                        return {};
                    }

                    rigid.first_vertex = std::min(rigid.first_vertex, vertex_index);
                    rigid.end_vertex = std::max(rigid.end_vertex, vertex_index + 1);
                }

                for (uint32_t previous = 0; previous < catalog.count; ++previous)
                {
                    const StudioRigidRange& other = catalog.ranges[previous];

                    if (rigid.first_vertex < other.end_vertex && other.first_vertex < rigid.end_vertex)
                    {
                        return {};
                    }
                }

                catalog.ranges[catalog.count++] = rigid;
            }

            catalog.index_count = end_index;

            return catalog;
        }

        void ConfigureVertexAttributes()
        {
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, position)));

            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, normal)));

            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, texel)));

            // The three bone bytes are adjacent, so they arrive as one integer vector rather
            // than as an attribute each.
            glEnableVertexAttribArray(3);
            glVertexAttribIPointer(3, 3, GL_UNSIGNED_BYTE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, position_bone)));

            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 3, GL_BYTE, GL_TRUE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, shell_normal)));
        }
    } // namespace

    struct StudioGeometry::State
    {
        ncl_studio::StudioMesh mesh_;
        GpuBuffer vertices_;
        GpuBuffer indices_;
        unsigned int vao_{};
        unsigned int rigid_vao_{};
        StudioGpuModel gpu_{};
        bool attempted_{};
        bool failed_{};

        explicit State(RenderDevice& device) :
            vertices_(device),
            indices_(device)
        {}
    };

    StudioGeometry::StudioGeometry(RenderDevice& device, performance::PerformanceMonitor& monitor) :
        device_(device),
        monitor_(monitor),
        state_(std::make_unique<State>(device))
    {}

    bool StudioGeometry::Upload()
    {
        state_->gpu_.rigid = BuildRigidCatalog(state_->mesh_);
        std::vector<GpuVertex> gpu_vertices;
        gpu_vertices.reserve(state_->mesh_.vertices.size());

        for (const ncl_studio::Vertex& source : state_->mesh_.vertices)
        {
            GpuVertex vertex{};
            vertex.position[0] = source.position.x;
            vertex.position[1] = source.position.y;
            vertex.position[2] = source.position.z;
            vertex.normal[0] = source.normal.x;
            vertex.normal[1] = source.normal.y;
            vertex.normal[2] = source.normal.z;
            vertex.shell_normal[0] = QuantizeNormal(source.shell_normal.x);
            vertex.shell_normal[1] = QuantizeNormal(source.shell_normal.y);
            vertex.shell_normal[2] = QuantizeNormal(source.shell_normal.z);
            vertex.texel[0] = source.texel.x;
            vertex.texel[1] = source.texel.y;
            vertex.position_bone = source.position_bone;
            vertex.normal_bone = source.normal_bone;
            vertex.shell_normal_bone = source.shell_normal_bone;

            auto bounds = std::find_if(state_->gpu_.bounds.begin(), state_->gpu_.bounds.end(), [&source](const StudioBoneBounds& box) {
                return box.bone == source.position_bone;
            });
            if (bounds == state_->gpu_.bounds.end())
            {
                state_->gpu_.bounds.push_back(
                    {source.position_bone,
                     {source.position.x, source.position.y, source.position.z},
                     {source.position.x, source.position.y, source.position.z}}
                );
            }
            else
            {
                for (int axis = 0; axis < 3; axis++)
                {
                    bounds->minimum[axis] = std::min(bounds->minimum[axis], vertex.position[axis]);
                    bounds->maximum[axis] = std::max(bounds->maximum[axis], vertex.position[axis]);
                }
            }

            gpu_vertices.push_back(vertex);
        }

        bool buffers_ok =
            state_->vertices_.Create(GL_ARRAY_BUFFER, gpu_vertices.data(), gpu_vertices.size() * sizeof(GpuVertex)) &&
            state_->indices_.Create(GL_ELEMENT_ARRAY_BUFFER, state_->mesh_.indices.data(), state_->mesh_.indices.size() * sizeof(uint32_t));

        // Last use of both arrays. Skinning happens in the vertex shader, so the buffers
        // are filled once and never rewritten; the draw path works from the ranges beside
        // them, and r_studio_info builds a copy of its own. Kept, they would hold 48 bytes
        // per vertex for as long as the model stays cached. Swapped rather than cleared,
        // because clear() keeps the capacity.
        std::vector<ncl_studio::Vertex>().swap(state_->mesh_.vertices);
        std::vector<uint32_t>().swap(state_->mesh_.indices);

        if (!buffers_ok)
        {
            return false;
        }

        GLint previous_vao = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vao);

        GLint previous_buffer = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_buffer);

        glGenVertexArrays(1, &state_->vao_);

        if (state_->vao_ == 0)
        {
            return false;
        }

        glBindVertexArray(state_->vao_);
        glBindBuffer(GL_ARRAY_BUFFER, state_->vertices_.get_name());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state_->indices_.get_name());

        ConfigureVertexAttributes();

        glBindVertexArray(static_cast<GLuint>(previous_vao));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_buffer));

        return true;
    }
    StudioGeometry::~StudioGeometry()
    {
        OnDeviceLost();
    }

    const StudioGpuModel* StudioGeometry::Prepare(const std::string& name, const goldsrc::studiohdr_t* header, size_t size)
    {
        performance::LocalCpuScope preparation(monitor_.get_scope_monitor(), performance::PerfStage::StudioGeometry);

        if (!device_.is_context_alive() || !device_.CheckAvailable())
        {
            return nullptr;
        }

        if (state_->attempted_)
        {
            if (!state_->failed_)
            {
                PrepareRigidVao();
            }

            return state_->failed_ ? nullptr : &state_->gpu_;
        }

        state_->attempted_ = true;
        state_->failed_ = true;
        ncl_utils::ResultT<ncl_studio::StudioMesh> mesh = ncl_studio::BuildStudioMesh(header, size);

        if (mesh.has_error())
        {
            Con_DPrintf(ConLogType::Error, "render: %s: %s\n", name.c_str(), mesh.error_str().c_str());
            return nullptr;
        }

        state_->mesh_ = *mesh;

        if (!Upload())
        {
            Con_DPrintf(ConLogType::Error, "render: %s: failed to upload studio geometry\n", name.c_str());
            return nullptr;
        }

        state_->failed_ = false;
        state_->gpu_.vao = state_->vao_;
        state_->gpu_.mesh = &state_->mesh_;
        PrepareRigidVao();

        return &state_->gpu_;
    }

    void StudioGeometry::PrepareRigidVao()
    {
        if (state_->rigid_vao_ != 0 || state_->gpu_.rigid.count == 0)
        {
            return;
        }

        GLint previous_vao = 0;
        GLint previous_buffer = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_buffer);
        glGenVertexArrays(1, &state_->rigid_vao_);

        if (state_->rigid_vao_ != 0)
        {
            glBindVertexArray(state_->rigid_vao_);
            glBindBuffer(GL_ARRAY_BUFFER, state_->vertices_.get_name());
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state_->indices_.get_name());
            ConfigureVertexAttributes();

            for (GLuint slot = kStudioInstanceMatrixSlot; slot <= kStudioInstanceRangeSlot; ++slot)
            {
                glEnableVertexAttribArray(slot);
                glVertexAttribDivisor(slot, 1);
            }

            state_->gpu_.rigid_vao = state_->rigid_vao_;
        }

        glBindVertexArray(static_cast<GLuint>(previous_vao));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_buffer));
    }

    void StudioGeometry::Release()
    {
        if (!device_.IsContextCurrent())
        {
            OnDeviceLost();
            return;
        }

        if (state_->vao_ != 0)
        {
            glDeleteVertexArrays(1, &state_->vao_);
        }
        if (state_->rigid_vao_ != 0)
        {
            glDeleteVertexArrays(1, &state_->rigid_vao_);
        }

        state_->vertices_.Destroy();
        state_->indices_.Destroy();
        OnDeviceLost();
    }

    void StudioGeometry::OnDeviceLost()
    {
        state_->vao_ = 0;
        state_->rigid_vao_ = 0;
        state_->vertices_.OnDeviceLost();
        state_->indices_.OnDeviceLost();
        state_->gpu_ = {};
        state_->mesh_ = {};
        state_->attempted_ = false;
        state_->failed_ = false;
    }
} // namespace render
