#include "graphics/render/StudioSubmit.h"

#include <algorithm>
#include <cassert>
#include <span>
#include <vector>

#include <ncl_gl/program.h>
#include <ncl_material/ProgramManifest.h>

#include "common/memory/ScratchArena.h"
#include "common/memory/g_scratch_allocator.h"
#include "performance/PerformanceMapper.h"
#include "performance/PerformanceMonitor.h"
#include "performance/performance_scopes.h"

namespace render
{
    namespace
    {
        static_assert(kStudioTouchedTextureUnits <= 16);
        constexpr size_t kDrawScratchPadding = 2 * (ScratchAllocator::kAlignment - 1);

        void SetEnabled(GLenum capability, bool enabled)
        {
            if (enabled)
            {
                glEnable(capability);
            }
            else
            {
                glDisable(capability);
            }
        }

        bool SameMaterial(const std::optional<ncl_material::Material>& left, const std::optional<ncl_material::Material>& right)
        {
            if (left.has_value() != right.has_value())
            {
                return false;
            }

            if (!left)
            {
                return true;
            }

            if (left->program != right->program || left->params.size() != right->params.size())
            {
                return false;
            }

            for (size_t i = 0; i < left->params.size(); i++)
            {
                const ncl_material::Param& a = left->params[i];
                const ncl_material::Param& b = right->params[i];

                if (a.name != b.name || a.value.type != b.value.type || a.value.scalar != b.value.scalar || a.value.vec4 != b.value.vec4 ||
                    a.value.texture_ref != b.value.texture_ref)
                {
                    return false;
                }
            }

            return true;
        }

        bool CanGroup(const StudioQueuedMesh& left, const StudioQueuedMesh& right)
        {
            return left.program == right.program && left.skin == right.skin && left.texture == right.texture &&
                   left.masked == right.masked && left.chrome == right.chrome && left.additive == right.additive &&
                   left.shade_mode == right.shade_mode && left.texel_scale[0] == right.texel_scale[0] &&
                   left.texel_scale[1] == right.texel_scale[1] && SameMaterial(left.material, right.material);
        }
    } // namespace

    StudioGlSubmission::StudioGlSubmission(
        RenderDevice& device,
        StudioMaterialRuntime& material_runtime,
        StudioSubmissionCallbacks callbacks,
        performance::PerformanceMonitor& monitor,
        performance::PerformanceMapper& mapper,
        unsigned int& studio_depth
    ) :
        monitor_(monitor),
        mapper_(mapper),
        studio_depth_(studio_depth),
        bones_(device),
        material_runtime_(material_runtime),
        callbacks_(callbacks)
    {}

    void StudioGlSubmission::Release()
    {
        bones_.Destroy();
        cpu_scope_.reset();
        bound_vao_ = 0;
        material_state_ = StudioMaterialState{};
        counts_.clear();
        offsets_.clear();
    }

    StudioCpuScope::StudioCpuScope(performance::PerformanceMonitor& monitor, unsigned int& depth, std::source_location location) :
        monitor_(monitor),
        depth_(depth)
    {
        if (depth_++ == 0)
        {
            token_ = monitor_.BeginCpu(performance::PerfStage::Studio, location);
        }
    }

    StudioCpuScope::~StudioCpuScope()
    {
        depth_--;

        if (token_ != 0)
        {
            monitor_.EndCpu(token_);
        }
    }

    bool StudioGlSubmission::EnsureBones()
    {
        return bones_.is_valid() || bones_.Create(GL_UNIFORM_BUFFER, nullptr, kStudioBoneFloats * sizeof(float), GL_DYNAMIC_DRAW);
    }

    void Studio_CaptureRasterState(StudioDrawPacket& packet, performance::PerformanceMonitor& monitor)
    {
        performance::LocalCpuScope cpu(monitor.get_studio_detail_monitor(), performance::PerfStage::StudioGlSave);
        performance::LocalCpuScope capture(monitor.get_studio_detail_monitor(), performance::PerfStage::StudioModelStateCapture);
        StudioRasterState& state = packet.raster;
        state.blend = glIsEnabled(GL_BLEND) != GL_FALSE;
        glGetIntegerv(GL_BLEND_SRC_RGB, &state.blend_src_rgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &state.blend_dst_rgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &state.blend_src_alpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &state.blend_dst_alpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &state.blend_equation_rgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &state.blend_equation_alpha);
        state.depth_test = glIsEnabled(GL_DEPTH_TEST) != GL_FALSE;
        GLboolean depth_write = GL_FALSE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
        state.depth_write = depth_write != GL_FALSE;
        glGetIntegerv(GL_DEPTH_FUNC, &state.depth_func);
        glGetDoublev(GL_DEPTH_RANGE, state.depth_range);
        state.cull = glIsEnabled(GL_CULL_FACE) != 0;
        glGetIntegerv(GL_CULL_FACE_MODE, &state.cull_face);
        glGetIntegerv(GL_FRONT_FACE, &state.front_face);
        glGetFloatv(GL_MODELVIEW_MATRIX, packet.modelview.data());
        glGetFloatv(GL_PROJECTION_MATRIX, packet.projection.data());
        glGetIntegerv(GL_VIEWPORT, packet.viewport.data());

        packet.fog = StudioFogState{};
        if (glIsEnabled(GL_FOG) != GL_FALSE)
        {
            glGetIntegerv(GL_FOG_MODE, &packet.fog.mode);
            glGetFloatv(GL_FOG_COLOR, packet.fog.color);
            glGetFloatv(GL_FOG_DENSITY, &packet.fog.density);
            glGetFloatv(GL_FOG_START, &packet.fog.start);
            glGetFloatv(GL_FOG_END, &packet.fog.end);
        }
    }

    void StudioGlSubmission::Begin()
    {
        cpu_scope_.emplace(monitor_, studio_depth_, std::source_location::current());
        assert(bones_.is_valid());

        {
            performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioGlSave);
            performance::LocalCpuScope capture(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioBatchStateSave);
            glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program_);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vao_);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_array_buffer_);
            glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &previous_uniform_buffer_);
            glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, kStudioBonesBinding, &previous_bones_);
            glGetInteger64i_v(GL_UNIFORM_BUFFER_START, kStudioBonesBinding, &previous_bones_start_);
            glGetInteger64i_v(GL_UNIFORM_BUFFER_SIZE, kStudioBonesBinding, &previous_bones_size_);
            glGetIntegerv(GL_MATRIX_MODE, &previous_matrix_mode_);

            glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT | GL_DEPTH_BUFFER_BIT | GL_CURRENT_BIT);
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();

            glActiveTexture(GL_TEXTURE0 + kStudioBaseTextureUnit);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture_);
        }

        {
            performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioBufferBind);
            glBindBufferBase(GL_UNIFORM_BUFFER, kStudioBonesBinding, bones_.get_name());
        }

        bound_vao_ = 0;
    }

    void Studio_ApplyFaceCulling(bool flipped)
    {
        SetEnabled(GL_CULL_FACE, !flipped);
        glCullFace(GL_FRONT);
    }

    void StudioGlSubmission::Draw(const StudioDrawPacket& packet)
    {
        if (packet.texture_revision != material_runtime_.get_texture_revision())
        {
            return;
        }
        assert(packet.bone_count > 0 && packet.bone_count <= goldsrc::MAXSTUDIOBONES);
        bool has_material =
            std::any_of(packet.meshes.begin(), packet.meshes.end(), [](const StudioQueuedMesh& mesh) { return mesh.material.has_value(); });
        // Authored shaders may consume bone entries beyond the model's vertex indices.
        size_t bone_floats = has_material ? kStudioBoneFloats : static_cast<size_t>(packet.bone_count) * 12;
        {
            performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioStateApply);
            SetEnabled(GL_BLEND, packet.raster.blend);
            glBlendFuncSeparate(
                packet.raster.blend_src_rgb, packet.raster.blend_dst_rgb, packet.raster.blend_src_alpha, packet.raster.blend_dst_alpha
            );
            glBlendEquationSeparate(packet.raster.blend_equation_rgb, packet.raster.blend_equation_alpha);
            SetEnabled(GL_DEPTH_TEST, packet.raster.depth_test);
            glDepthMask(packet.raster.depth_write ? GL_TRUE : GL_FALSE);
            glDepthFunc(packet.raster.depth_func);
            glDepthRange(packet.raster.depth_range[0], packet.raster.depth_range[1]);
            Studio_ApplyFaceCulling(packet.flipped);
            glMatrixMode(GL_MODELVIEW);
            glLoadMatrixf(packet.modelview.data());
            glMatrixMode(GL_PROJECTION);
            glLoadMatrixf(packet.projection.data());
        }

        {
            performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioBoneUpload);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(bone_floats * sizeof(float)), packet.bones.data());
        }

        if (bound_vao_ != packet.vao)
        {
            performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioBufferBind);
            glBindVertexArray(packet.vao);
            bound_vao_ = packet.vao;
        }

        ScratchArena arena(g_ScratchAllocator);
        size_t mesh_count = packet.meshes.size();
        size_t available = g_ScratchAllocator.GetCapacity() - g_ScratchAllocator.GetOffset();
        size_t array_bytes = mesh_count * (sizeof(GLsizei) + sizeof(const void*));
        std::span<GLsizei> counts;
        std::span<const void*> offsets;
        {
            performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioDataPrepare);
            // Material loading uses the same arena, so authored draws retain their own arrays.
            if (!has_material && array_bytes <= available && kDrawScratchPadding <= available - array_bytes)
            {
                counts = {arena.AllocateArray<GLsizei>(mesh_count), mesh_count};
                offsets = {arena.AllocateArray<const void*>(mesh_count), mesh_count};
            }
            else
            {
                counts_.resize(mesh_count);
                offsets_.resize(mesh_count);
                counts = counts_;
                offsets = offsets_;
            }
        }

        // Rebinding at packet boundaries keeps consecutive poses independent on compatibility drivers.
        const ncl_gl::Program* bound_program = nullptr;

        for (size_t start = 0; start < mesh_count;)
        {
            const StudioQueuedMesh& head = packet.meshes[start];
            const ncl_gl::Program& program = *head.program;

            if (bound_program != head.program)
            {
                {
                    performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioProgramBind);
                    glUseProgram(program.prog);
                }

                bound_program = head.program;

                if (callbacks_.note_program_bind != nullptr)
                {
                    callbacks_.note_program_bind(callbacks_.user);
                }

                performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioUniformSetup);

                material_runtime_.ConfigureProgram(*head.material_bindings);

                glUniform1f(program.loc[kUniformAmbient], packet.lighting.ambient);
                glUniform1f(program.loc[kUniformShade], packet.lighting.shade);
                glUniform3fv(program.loc[kUniformColorMix], 1, packet.lighting.color);
                glUniform3fv(program.loc[kUniformLightVec], 1, packet.lighting.light_vec);
                glUniform3fv(program.loc[kUniformChromeOrigin], 1, packet.chrome_origin.data());
                glUniform3fv(program.loc[kUniformViewRight], 1, packet.view_right.data());
                glUniform1f(program.loc[kUniformBlend], packet.blend);
                glUniform1f(program.loc[kUniformGlowScale], packet.glow_scale);
                glUniform3fv(program.loc[kUniformShellColor], 1, packet.shell_color.data());
                glUniform2fv(program.loc[kUniformGamma], 1, packet.gamma);
                glUniform1i(program.loc[kUniformFlipped], packet.flipped ? 1 : 0);
                glUniform1f(program.loc[kUniformBoneScale], packet.bone_scale);
                glUniform4f(
                    program.loc[kUniformEffectViewport],
                    static_cast<float>(packet.viewport[0]),
                    static_cast<float>(packet.viewport[1]),
                    static_cast<float>(packet.viewport[2]),
                    static_cast<float>(packet.viewport[3])
                );
                glUniform1f(program.loc[kUniformEffectTime], packet.effect_time);
                glUniform1i(program.loc[kUniformFogMode], packet.fog.mode);
                glUniform3fv(program.loc[kUniformFogColor], 1, packet.fog.color);
                glUniform3f(program.loc[kUniformFogParams], packet.fog.density, packet.fog.start, packet.fog.end);
                glUniform1i(program.loc[kUniformEntityLightCount], packet.light_count);

                if (packet.light_count != 0)
                {
                    glUniform4fv(program.loc[kUniformEntityLight], packet.light_count, &packet.lights[0][0]);
                    glUniform3fv(program.loc[kUniformEntityLightColor], packet.light_count, &packet.light_colors[0][0]);
                }
            }

            size_t end = start;
            size_t group_count = 0;
            uint64_t group_indices = 0;
            {
                performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioDataPrepare);
                do
                {
                    const StudioQueuedMesh& mesh = packet.meshes[end++];
                    group_indices += static_cast<uint64_t>(mesh.index_count);
                    counts[group_count] = mesh.index_count;
                    offsets[group_count] = reinterpret_cast<const void*>(mesh.index_offset);
                    ++group_count;
                } while (end < mesh_count && CanGroup(head, packet.meshes[end]));
            }

            {
                performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioStateApply);
                SetEnabled(GL_BLEND, packet.raster.blend);
                glBlendFuncSeparate(
                    packet.raster.blend_src_rgb, packet.raster.blend_dst_rgb, packet.raster.blend_src_alpha, packet.raster.blend_dst_alpha
                );
                glDepthMask(packet.raster.depth_write ? GL_TRUE : GL_FALSE);
                SetEnabled(GL_ALPHA_TEST, head.masked);

                if (head.masked)
                {
                    glAlphaFunc(GL_GREATER, 0.5f);
                    glDepthMask(GL_TRUE);
                }

                if (head.additive)
                {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_ONE, GL_ONE);
                    glDepthMask(GL_FALSE);
                }
            }

            int material_textures = 0;

            if (head.material)
            {
                material_textures = material_runtime_.UploadMaterial(*head.material_bindings, *head.material, head.images, material_state_);
            }

            {
                performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioTextureSetup);
                callbacks_.bind_texture(callbacks_.user, head.texture);
            }

            {
                performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioUniformSetup);
                glUniform2f(program.loc[kUniformTexelScale], head.texel_scale[0], head.texel_scale[1]);
                glUniform1i(program.loc[kUniformShadeMode], head.shade_mode);
                glUniform1i(program.loc[kUniformChrome], head.chrome ? 1 : 0);
            }

            performance::PerformanceMonitor* monitor = monitor_.get_counter_monitor();
            {
                performance::LocalCpuScope cpu(monitor, performance::PerfStage::StudioShader);
                performance::LocalGpuScope gpu(monitor, performance::PerfStage::StudioShader);
                glMultiDrawElements(GL_TRIANGLES, counts.data(), GL_UNSIGNED_INT, offsets.data(), static_cast<GLsizei>(group_count));
            }

            performance::reporting::ReportDraw(monitor_, group_indices, group_count);
            mapper_.AddShaderTriangles(group_indices / 3);

            {
                performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioTextureSetup);

                for (int i = 0; i < material_textures; i++)
                {
                    glActiveTexture(GL_TEXTURE0 + kFirstMaterialUnit + i);
                    glBindTexture(GL_TEXTURE_2D, 0);
                }

                glActiveTexture(GL_TEXTURE0 + kStudioBaseTextureUnit);
            }

            start = end;
        }
    }

    void StudioGlSubmission::End()
    {
        {
            performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioGlRestore);
            {
                performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioRestoreBindings);
                glBindVertexArray(static_cast<GLuint>(previous_vao_));

                if (previous_bones_ != 0 && previous_bones_size_ != 0)
                {
                    glBindBufferRange(
                        GL_UNIFORM_BUFFER,
                        kStudioBonesBinding,
                        static_cast<GLuint>(previous_bones_),
                        static_cast<GLintptr>(previous_bones_start_),
                        static_cast<GLsizeiptr>(previous_bones_size_)
                    );
                }
                else
                {
                    glBindBufferBase(GL_UNIFORM_BUFFER, kStudioBonesBinding, static_cast<GLuint>(previous_bones_));
                }

                glBindBuffer(GL_UNIFORM_BUFFER, static_cast<GLuint>(previous_uniform_buffer_));
                glUseProgram(static_cast<GLuint>(previous_program_));
            }

            {
                performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioRestoreMatrices);
                glMatrixMode(GL_PROJECTION);
                glPopMatrix();
                glMatrixMode(GL_MODELVIEW);
                glPopMatrix();
                glMatrixMode(static_cast<GLenum>(previous_matrix_mode_));
                glPopAttrib();
            }

            {
                performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioRestoreBindings);
                glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_array_buffer_));
            }

            {
                performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioRestoreTextures);
                material_state_.Restore();

                glActiveTexture(GL_TEXTURE0 + kStudioBaseTextureUnit);
                callbacks_.bind_texture(callbacks_.user, static_cast<GLuint>(previous_texture_));
            }
        }

        cpu_scope_.reset();
    }
} // namespace render
