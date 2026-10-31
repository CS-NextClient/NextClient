#include "graphics/render/StudioSubmit.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <span>
#include <type_traits>
#include <vector>

#include <ncl_gl/program.h>
#include <ncl_material/ProgramManifest.h>

#include "common/memory/ScratchArena.h"
#include "common/memory/g_scratch_allocator.h"
#include "graphics/render/StudioPhaseCapture.h"
#include "performance/PerformanceMapper.h"
#include "performance/PerformanceMonitor.h"
#include "performance/performance_scopes.h"

namespace render
{
    namespace
    {
        static_assert(kStudioTouchedTextureUnits <= 16);
        constexpr size_t kDrawScratchPadding = 2 * (ScratchAllocator::kAlignment - 1);

        template <typename T>
        bool IsFiniteValue(T value)
        {
            return std::isfinite(value);
        }

        template <typename T, size_t N>
        bool IsFiniteValue(const T (&values)[N])
        {
            return std::all_of(values, values + N, [](T value) { return IsFiniteValue(value); });
        }

        template <typename T, size_t N>
        bool IsFiniteValue(const std::array<T, N>& values)
        {
            return std::all_of(values.begin(), values.end(), [](T value) { return IsFiniteValue(value); });
        }

        bool IsFiniteValue(const ncl_math::Vector3& value)
        {
            return IsFiniteValue(value.x) && IsFiniteValue(value.y) && IsFiniteValue(value.z);
        }

        bool IsSharedStateFinite(const StudioDrawPacket& packet)
        {
            return IsFiniteValue(packet.modelview) &&
                   IsFiniteValue(packet.projection) &&
                   IsFiniteValue(packet.raster.depth_range) &&
                   IsFiniteValue(packet.fog.color) &&
                   IsFiniteValue(packet.fog.density) &&
                   IsFiniteValue(packet.fog.start) &&
                   IsFiniteValue(packet.fog.end) &&
                   IsFiniteValue(packet.lighting.ambient) &&
                   IsFiniteValue(packet.lighting.shade) &&
                   IsFiniteValue(packet.lighting.color) &&
                   IsFiniteValue(packet.lighting.light_vec) &&
                   IsFiniteValue(packet.chrome_origin) &&
                   IsFiniteValue(packet.view_right) &&
                   IsFiniteValue(packet.glow_scale) &&
                   IsFiniteValue(packet.bone_scale) &&
                   IsFiniteValue(packet.shell_color) &&
                   IsFiniteValue(packet.gamma) &&
                   IsFiniteValue(packet.effect_time);
        }

        template <typename T>
        bool AreValidatedValuesEqual(const T& left, const T& right)
        {
            if constexpr (std::is_floating_point_v<T>)
            {
                return std::memcmp(&left, &right, sizeof(T)) == 0;
            }
            else
            {
                return left == right;
            }
        }

        template <typename T, size_t N>
        bool AreValidatedValuesEqual(const T (&left)[N], const T (&right)[N])
        {
            for (size_t index = 0; index < N; ++index)
            {
                if (!AreValidatedValuesEqual(left[index], right[index]))
                {
                    return false;
                }
            }

            return true;
        }

        template <typename T, size_t N>
        bool AreValidatedValuesEqual(const std::array<T, N>& left, const std::array<T, N>& right)
        {
            for (size_t index = 0; index < N; ++index)
            {
                if (!AreValidatedValuesEqual(left[index], right[index]))
                {
                    return false;
                }
            }

            return true;
        }

        bool AreValidatedValuesEqual(const ncl_math::Vector3& left, const ncl_math::Vector3& right)
        {
            return AreValidatedValuesEqual(left.x, right.x) && AreValidatedValuesEqual(left.y, right.y) &&
                   AreValidatedValuesEqual(left.z, right.z);
        }

        bool AreValidatedStatesEqual(const StudioDrawPacket& left, const StudioDrawPacket& right)
        {
            return AreValidatedValuesEqual(left.modelview, right.modelview) &&
                   AreValidatedValuesEqual(left.projection, right.projection) &&
                   AreValidatedValuesEqual(left.viewport, right.viewport) &&
                   AreValidatedValuesEqual(left.texture_revision, right.texture_revision) &&
                   AreValidatedValuesEqual(left.raster.blend, right.raster.blend) &&
                   AreValidatedValuesEqual(left.raster.blend_src_rgb, right.raster.blend_src_rgb) &&
                   AreValidatedValuesEqual(left.raster.blend_dst_rgb, right.raster.blend_dst_rgb) &&
                   AreValidatedValuesEqual(left.raster.blend_src_alpha, right.raster.blend_src_alpha) &&
                   AreValidatedValuesEqual(left.raster.blend_dst_alpha, right.raster.blend_dst_alpha) &&
                   AreValidatedValuesEqual(left.raster.blend_equation_rgb, right.raster.blend_equation_rgb) &&
                   AreValidatedValuesEqual(left.raster.blend_equation_alpha, right.raster.blend_equation_alpha) &&
                   AreValidatedValuesEqual(left.raster.cull, right.raster.cull) &&
                   AreValidatedValuesEqual(left.raster.cull_face, right.raster.cull_face) &&
                   AreValidatedValuesEqual(left.raster.front_face, right.raster.front_face) &&
                   AreValidatedValuesEqual(left.raster.depth_test, right.raster.depth_test) &&
                   AreValidatedValuesEqual(left.raster.depth_write, right.raster.depth_write) &&
                   AreValidatedValuesEqual(left.raster.depth_func, right.raster.depth_func) &&
                   AreValidatedValuesEqual(left.raster.depth_range, right.raster.depth_range) &&
                   AreValidatedValuesEqual(left.fog.mode, right.fog.mode) &&
                   AreValidatedValuesEqual(left.fog.color, right.fog.color) &&
                   AreValidatedValuesEqual(left.fog.density, right.fog.density) &&
                   AreValidatedValuesEqual(left.fog.start, right.fog.start) &&
                   AreValidatedValuesEqual(left.fog.end, right.fog.end) &&
                   AreValidatedValuesEqual(left.lighting.ambient, right.lighting.ambient) &&
                   AreValidatedValuesEqual(left.lighting.shade, right.lighting.shade) &&
                   AreValidatedValuesEqual(left.lighting.color, right.lighting.color) &&
                   AreValidatedValuesEqual(left.lighting.light_vec, right.lighting.light_vec) &&
                   AreValidatedValuesEqual(left.chrome_origin, right.chrome_origin) &&
                   AreValidatedValuesEqual(left.view_right, right.view_right) &&
                   AreValidatedValuesEqual(left.flipped, right.flipped) &&
                   AreValidatedValuesEqual(left.glow_scale, right.glow_scale) &&
                   AreValidatedValuesEqual(left.bone_scale, right.bone_scale) &&
                   AreValidatedValuesEqual(left.shell_color, right.shell_color) &&
                   AreValidatedValuesEqual(left.light_count, right.light_count) &&
                   AreValidatedValuesEqual(left.gamma, right.gamma) &&
                   AreValidatedValuesEqual(left.effect_time, right.effect_time);
        }

        bool AreRigidMeshesCompatible(const StudioDrawPacket& left, const StudioDrawPacket& right)
        {
            const StudioQueuedMesh& a = left.meshes[0];
            const StudioQueuedMesh& b = right.meshes[0];

            return left.vao == right.vao &&
                   left.rigid_vao == right.rigid_vao &&
                   left.rigid_bindings == right.rigid_bindings &&
                   left.rigid_index_count == right.rigid_index_count &&
                   a.program == b.program &&
                   a.material_bindings == b.material_bindings &&
                   a.texture == b.texture &&
                   a.skin == b.skin &&
                   AreValidatedValuesEqual(a.texel_scale, b.texel_scale) &&
                   a.masked == b.masked &&
                   a.additive == b.additive &&
                   a.shade_mode == b.shade_mode &&
                   a.images.manifest == b.images.manifest &&
                   a.images.material == b.images.material &&
                   a.images.material_count == b.images.material_count &&
                   a.images.coverage_dissolve_map == b.images.coverage_dissolve_map &&
                   AreValidatedStatesEqual(left, right);
        }

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
        unsigned int& studio_depth,
        StudioPhaseCapture* phase_capture
    ) :
        monitor_(monitor),
        mapper_(mapper),
        studio_depth_(studio_depth),
        phase_capture_(phase_capture),
        bones_(device),
        instances_(device),
        material_runtime_(material_runtime),
        callbacks_(callbacks)
    {}

    void StudioGlSubmission::Release()
    {
        bones_.Destroy();
        instances_.Destroy();
        cpu_scope_.reset();
        bound_vao_ = 0;
        material_state_ = StudioMaterialState{};
        ResetStateCache();
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

    bool StudioGlSubmission::EnsureInstances()
    {
        static_assert(sizeof(InstanceRecord) == 64);
        static_assert(offsetof(InstanceRecord, first_vertex) == 48);
        static_assert(offsetof(InstanceRecord, end_vertex) == 52);
        static_assert(offsetof(InstanceRecord, blend_bits) == 56);
        static_assert(offsetof(InstanceRecord, padding) == 60);

        return instances_.is_valid() || instances_.Create(GL_ARRAY_BUFFER, nullptr, sizeof(instance_records_), GL_DYNAMIC_DRAW);
    }

    void StudioGlSubmission::ResetStats()
    {
        stats_ = StudioSubmissionStats{};
    }

    bool StudioGlSubmission::IsRigidEligible(const StudioDrawPacket& packet) const
    {
        if (packet.rigid_vao == 0 || packet.rigid_bindings == nullptr || packet.rigid_bindings->program == nullptr ||
            packet.rigid_bindings->generation != material_runtime_.get_shader_generation() ||
            packet.texture_revision != material_runtime_.get_texture_revision() || packet.meshes.size() != 1 || packet.flipped ||
            !std::isfinite(packet.blend) || packet.bone_scale != 1.0f || packet.glow_scale != 0.0f || packet.light_count != 0 ||
            packet.bone_count <= 0 || packet.bone_count > goldsrc::MAXSTUDIOBONES ||
            packet.rigid_range.bone >= static_cast<uint32_t>(packet.bone_count) || packet.rigid_index_count == 0 ||
            packet.rigid_range.first_vertex >= packet.rigid_range.end_vertex)
        {
            return false;
        }

        const StudioQueuedMesh& mesh = packet.meshes[0];

        if (mesh.material || mesh.chrome || mesh.shade_mode == kShadeGlowShell || mesh.material_bindings == nullptr ||
            mesh.material_bindings->generation != packet.rigid_bindings->generation ||
            mesh.index_count != static_cast<int>(packet.rigid_range.index_count) ||
            mesh.index_offset != static_cast<uintptr_t>(packet.rigid_range.first_index) * sizeof(uint32_t) ||
            !IsFiniteValue(mesh.texel_scale) || !IsSharedStateFinite(packet))
        {
            return false;
        }

        const float* rows = packet.bones.data() + packet.rigid_range.bone * 12;

        return std::all_of(rows, rows + 12, [](float value) { return std::isfinite(value); });
    }

    void StudioGlSubmission::BindInstances(const StudioDrawPacket& packet, size_t first)
    {
        performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioBufferBind);

        if (bound_vao_ != packet.rigid_vao)
        {
            glBindVertexArray(packet.rigid_vao);
            bound_vao_ = packet.rigid_vao;
        }

        glBindBuffer(GL_ARRAY_BUFFER, instances_.get_name());
        const uintptr_t base = first * sizeof(InstanceRecord);

        for (GLuint row = 0; row < kStudioInstanceMatrixRows; ++row)
        {
            glVertexAttribPointer(
                kStudioInstanceMatrixSlot + row,
                4,
                GL_FLOAT,
                GL_FALSE,
                sizeof(InstanceRecord),
                reinterpret_cast<const void*>(base + row * 16)
            );
        }

        glVertexAttribIPointer(
            kStudioInstanceRangeSlot,
            3,
            GL_UNSIGNED_INT,
            sizeof(InstanceRecord),
            reinterpret_cast<const void*>(base + offsetof(InstanceRecord, first_vertex))
        );
    }

    void StudioGlSubmission::DrawRun(std::span<const StudioDrawPacket> packets)
    {
        if (!instances_.is_valid())
        {
            stats_.fallbacks[static_cast<size_t>(StudioRigidFallback::Resource)] += packets.size();
            StudioSubmissionSink::DrawRun(packets);

            return;
        }

        if (glIsEnabled(GL_PRIMITIVE_RESTART) != GL_FALSE)
        {
            stats_.fallbacks[static_cast<size_t>(StudioRigidFallback::PrimitiveRestart)] += packets.size();
            StudioSubmissionSink::DrawRun(packets);

            return;
        }

        assert(packets.size() <= kMaxStudioPackets);

        std::array<size_t, kMaxStudioPackets> lengths{};
        bool has_batch = false;

        {
            performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioInstancePlan);

            enum class Eligibility : uint8_t
            {
                Unchecked,
                Ineligible,
                Eligible
            };

            std::array<Eligibility, kMaxStudioPackets> eligibility{};
            const auto is_eligible = [&](size_t index) {
                Eligibility& result = eligibility[index];

                if (result == Eligibility::Unchecked)
                {
                    result = IsRigidEligible(packets[index]) ? Eligibility::Eligible : Eligibility::Ineligible;
                }

                return result == Eligibility::Eligible;
            };

            for (size_t first = 0; first < packets.size();)
            {
                if (!is_eligible(first))
                {
                    ++stats_.fallbacks[static_cast<size_t>(StudioRigidFallback::Ineligible)];
                    ++first;

                    continue;
                }

                size_t end = first + 1;
                while (end < packets.size() && is_eligible(end) && AreRigidMeshesCompatible(packets[first], packets[end]))
                {
                    ++end;
                }

                if (end - first >= 2)
                {
                    lengths[first] = end - first;
                    has_batch = true;
                }
                else
                {
                    ++stats_.fallbacks[static_cast<size_t>(
                        end < packets.size() ? StudioRigidFallback::SharedState : StudioRigidFallback::Singleton
                    )];
                }

                first = end;
            }

            if (has_batch)
            {
                std::fill_n(instance_records_.begin(), packets.size(), InstanceRecord{});

                for (size_t first = 0; first < packets.size(); ++first)
                {
                    for (size_t index = first; index < first + lengths[first]; ++index)
                    {
                        const StudioDrawPacket& packet = packets[index];
                        InstanceRecord& record = instance_records_[index];

                        std::copy_n(packet.bones.data() + packet.rigid_range.bone * 12, 12, record.rows);
                        record.first_vertex = packet.rigid_range.first_vertex;
                        record.end_vertex = packet.rigid_range.end_vertex;
                        std::memcpy(&record.blend_bits, &packet.blend, sizeof(record.blend_bits));
                    }
                }
            }
        }

        if (!has_batch)
        {
            StudioSubmissionSink::DrawRun(packets);

            return;
        }

        glBindBuffer(GL_ARRAY_BUFFER, instances_.get_name());
        const GLsizeiptr bytes = static_cast<GLsizeiptr>(packets.size() * sizeof(InstanceRecord));

        {
            performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioInstanceUpload);
            glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, instance_records_.data());
        }

        ++stats_.instance_uploads;
        stats_.instance_bytes += bytes;

        for (size_t first = 0; first < packets.size();)
        {
            const size_t count = lengths[first];

            if (count < 2)
            {
                Draw(packets[first++]);

                continue;
            }

            BindInstances(packets[first], first);
            DrawPacket(packets[first], count);

            ++stats_.instance_batches;
            stats_.saved_calls += count - 1;
            first += count;
        }
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
        ResetStateCache();
    }

    void StudioGlSubmission::ResetStateCache()
    {
        raster_valid_ = false;
        alpha_test_.reset();
        alpha_func_set_ = false;
        bound_texture_.reset();
    }

    void StudioGlSubmission::ApplyBlend(bool enabled, int src_rgb, int dst_rgb, int src_alpha, int dst_alpha)
    {
        if (!raster_valid_ || applied_raster_.blend != enabled)
        {
            SetEnabled(GL_BLEND, enabled);
            applied_raster_.blend = enabled;
        }

        if (!raster_valid_ || applied_raster_.blend_src_rgb != src_rgb || applied_raster_.blend_dst_rgb != dst_rgb ||
            applied_raster_.blend_src_alpha != src_alpha || applied_raster_.blend_dst_alpha != dst_alpha)
        {
            glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
            applied_raster_.blend_src_rgb = src_rgb;
            applied_raster_.blend_dst_rgb = dst_rgb;
            applied_raster_.blend_src_alpha = src_alpha;
            applied_raster_.blend_dst_alpha = dst_alpha;
        }
    }

    void StudioGlSubmission::ApplyDepthWrite(bool enabled)
    {
        if (!raster_valid_ || applied_raster_.depth_write != enabled)
        {
            glDepthMask(enabled ? GL_TRUE : GL_FALSE);
            applied_raster_.depth_write = enabled;
        }
    }

    void StudioGlSubmission::ApplyAlphaTest(bool enabled)
    {
        if (!alpha_test_.has_value() || *alpha_test_ != enabled)
        {
            SetEnabled(GL_ALPHA_TEST, enabled);
            alpha_test_ = enabled;
        }

        if (enabled && !alpha_func_set_)
        {
            glAlphaFunc(GL_GREATER, 0.5f);
            alpha_func_set_ = true;
        }
    }

    void StudioGlSubmission::ApplyRasterState(const StudioDrawPacket& packet)
    {
        const StudioRasterState& state = packet.raster;

        ApplyBlend(
            state.blend,
            state.blend_src_rgb,
            state.blend_dst_rgb,
            state.blend_src_alpha,
            state.blend_dst_alpha
        );
        ApplyDepthWrite(state.depth_write);

        if (!raster_valid_ || applied_raster_.blend_equation_rgb != state.blend_equation_rgb ||
            applied_raster_.blend_equation_alpha != state.blend_equation_alpha)
        {
            glBlendEquationSeparate(state.blend_equation_rgb, state.blend_equation_alpha);
            applied_raster_.blend_equation_rgb = state.blend_equation_rgb;
            applied_raster_.blend_equation_alpha = state.blend_equation_alpha;
        }

        if (!raster_valid_ || applied_raster_.depth_test != state.depth_test)
        {
            SetEnabled(GL_DEPTH_TEST, state.depth_test);
            applied_raster_.depth_test = state.depth_test;
        }

        if (!raster_valid_ || applied_raster_.depth_func != state.depth_func)
        {
            glDepthFunc(state.depth_func);
            applied_raster_.depth_func = state.depth_func;
        }

        if (!raster_valid_ || applied_raster_.depth_range[0] != state.depth_range[0] ||
            applied_raster_.depth_range[1] != state.depth_range[1])
        {
            glDepthRange(state.depth_range[0], state.depth_range[1]);
            applied_raster_.depth_range[0] = state.depth_range[0];
            applied_raster_.depth_range[1] = state.depth_range[1];
        }

        if (!raster_valid_ || applied_raster_.cull != !packet.flipped)
        {
            SetEnabled(GL_CULL_FACE, !packet.flipped);
            applied_raster_.cull = !packet.flipped;
        }

        if (!raster_valid_)
        {
            glCullFace(GL_FRONT);
        }

        raster_valid_ = true;
    }

    void Studio_ApplyFaceCulling(bool flipped)
    {
        SetEnabled(GL_CULL_FACE, !flipped);
        glCullFace(GL_FRONT);
    }

    void StudioGlSubmission::Draw(const StudioDrawPacket& packet)
    {
        DrawPacket(packet, 0);
    }

    void StudioGlSubmission::DrawPacket(const StudioDrawPacket& packet, size_t instances)
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
            ApplyRasterState(packet);
            glMatrixMode(GL_MODELVIEW);
            glLoadMatrixf(packet.modelview.data());
            glMatrixMode(GL_PROJECTION);
            glLoadMatrixf(packet.projection.data());
        }

        if (instances == 0)
        {
            performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioBoneUpload);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(bone_floats * sizeof(float)), packet.bones.data());
        }

        if (instances == 0 && bound_vao_ != packet.vao)
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
            const StudioMaterialBindings& bindings = instances != 0 ? *packet.rigid_bindings : *head.material_bindings;
            const ncl_gl::Program& program = *bindings.program;

            if (bound_program != &program)
            {
                {
                    performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioProgramBind);
                    glUseProgram(program.prog);
                }

                bound_program = &program;

                if (callbacks_.note_program_bind != nullptr)
                {
                    callbacks_.note_program_bind(callbacks_.user);
                }

                performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioUniformSetup);

                material_runtime_.ConfigureProgram(bindings);

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
                ApplyBlend(
                    packet.raster.blend,
                    packet.raster.blend_src_rgb,
                    packet.raster.blend_dst_rgb,
                    packet.raster.blend_src_alpha,
                    packet.raster.blend_dst_alpha
                );
                ApplyDepthWrite(packet.raster.depth_write);
                ApplyAlphaTest(head.masked);

                if (head.masked)
                {
                    ApplyDepthWrite(true);
                }

                if (head.additive)
                {
                    ApplyBlend(true, GL_ONE, GL_ONE, GL_ONE, GL_ONE);
                    ApplyDepthWrite(false);
                }
            }

            int material_textures = 0;

            if (head.material)
            {
                material_textures = material_runtime_.UploadMaterial(*head.material_bindings, *head.material, head.images, material_state_);
            }

            {
                performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioTextureSetup);
                if (!bound_texture_.has_value() || *bound_texture_ != head.texture)
                {
                    callbacks_.bind_texture(callbacks_.user, head.texture);
                    bound_texture_ = head.texture;
                }
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
                if (instances != 0)
                {
                    performance::LocalCpuScope submit(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioInstanceSubmit);
                    glDrawElementsInstanced(
                        GL_TRIANGLES,
                        static_cast<GLsizei>(packet.rigid_index_count),
                        GL_UNSIGNED_INT,
                        nullptr,
                        static_cast<GLsizei>(instances)
                    );
                }
                else
                {
                    glMultiDrawElements(GL_TRIANGLES, counts.data(), GL_UNSIGNED_INT, offsets.data(), static_cast<GLsizei>(group_count));
                }
            }

            const uint64_t logical_indices = group_indices * (instances != 0 ? instances : 1);
            const uint64_t submitted_indices = instances != 0 ? packet.rigid_index_count * instances : logical_indices;
            ++stats_.physical_draws;
            stats_.logical_groups += instances != 0 ? instances : 1;
            stats_.logical_triangles += logical_indices / 3;
            stats_.submitted_triangles += submitted_indices / 3;

            if (instances != 0)
            {
                monitor_.AddCounter(performance::PerfCounterId::StudioDrawCalls, 1);
                monitor_.AddCounter(performance::PerfCounterId::StudioConstituentDraws, instances);
                monitor_.AddCounter(performance::PerfCounterId::StudioTriangles, logical_indices / 3);
                monitor_.AddCounter(performance::PerfCounterId::StudioSingleMeshes, instances);
            }
            else
            {
                performance::reporting::ReportDraw(monitor_, logical_indices, group_count);
            }

            if (phase_capture_ != nullptr && phase_capture_->is_active())
            {
                phase_capture_->NotePhysicalDraw(submitted_indices / 3);
                phase_capture_->NoteGroup(
                    instances != 0 ? instances : 1, group_count * (instances != 0 ? instances : 1), logical_indices / 3
                );
            }
            mapper_.AddShaderTriangles(logical_indices / 3);

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

        ResetStateCache();
        cpu_scope_.reset();
    }
} // namespace render
