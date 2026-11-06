#include "graphics/preview/PreviewInteraction.h"

#include <algorithm>
#include <array>

#include <glad/glad.h>

#include "graphics/render/ScopedGlState.h"
#include "graphics/render/material/MaterialCoverageRuntime.h"
#include "graphics/render/ShaderManager.h"
#include "graphics/render/StudioSubmit.h"
#include "graphics/render/material/StudioProgramContract.h"

namespace render::preview
{
    namespace
    {
        constexpr const char* kPickUniforms[] =
            {"uTexelScale", "uBase", "uMasked", "uId", "uChrome", "uChromeOrigin", "uViewRight", "uAlphaScale"};
        constexpr const char* kOutlineUniforms[] = {"uIds", "uId", "uOrigin"};
        constexpr render::ProgramDesc kPickProgram{"preview_pick.vert", "preview_pick.frag", nullptr, kPickUniforms, 8, nullptr, 0};
        constexpr render::ProgramDesc
            kOutlineProgram{"preview_outline.vert", "preview_outline.frag", nullptr, kOutlineUniforms, 3, nullptr, 0};
        constexpr GLuint kEmptyPickId[4]{};
    } // namespace

    PreviewInteraction::PreviewInteraction(RenderDevice& device, ShaderManager& shaders, MaterialCoverageRuntime& coverage) :
        device_(device),
        shaders_(shaders),
        coverage_(coverage)
    {
        device_.Register(this);
    }

    PreviewInteraction::~PreviewInteraction()
    {
        OnDeviceLost();
        device_.Unregister(this);
    }

    void PreviewInteraction::BeginFrame()
    {
        for (Packet& packet : packets_)
        {
            mesh_storage_.Recycle(packet.draw.meshes);
        }

        packets_.clear();
        identities_.clear();
    }

    void PreviewInteraction::DiscardPackets()
    {
        packets_.clear();
        mesh_storage_.Clear();
        identities_.clear();
    }

    void PreviewInteraction::BeginModel(int index, const PreviewModel& model)
    {
        model_ = index;
        camera_space_ = model.camera_space;
        mirror_ = model.camera_space && model.mirror;
    }

    void PreviewInteraction::Capture(const render::StudioDrawPacket& packet)
    {
        StudioDrawPacket captured;
        captured.meshes = mesh_storage_.Acquire();
        captured = packet;
        packets_.push_back({std::move(captured), model_, camera_space_, mirror_});
    }

    bool PreviewInteraction::EnsureTarget(int wide, int tall)
    {
        if (framebuffer_ != 0 && wide_ == wide && tall_ == tall)
        {
            return true;
        }

        if (framebuffer_ == 0)
        {
            glGenFramebuffers(1, &framebuffer_);
            glGenTextures(1, &ids_);
            glGenRenderbuffers(1, &depth_);
            glGenBuffers(1, &bones_);
            glGenVertexArrays(1, &vao_);
            glBindBuffer(GL_UNIFORM_BUFFER, bones_);
            glBufferData(GL_UNIFORM_BUFFER, render::kStudioBoneFloats * sizeof(float), nullptr, GL_STREAM_DRAW);
        }

        glActiveTexture(GL_TEXTURE0 + render::kStudioBaseTextureUnit);
        glBindTexture(GL_TEXTURE_2D, ids_);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, wide, tall, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, wide, tall);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ids_, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            return false;
        }

        wide_ = wide;
        tall_ = tall;

        return true;
    }

    PreviewHit PreviewInteraction::Render(const PreviewRect& view, const PreviewRect& clip, int x, int y)
    {
        PreviewHit hit{};

        if (packets_.empty() || x < 0 || y < 0 || x >= view.wide || y >= view.tall || x + view.x < clip.x || y + view.y < clip.y ||
            x + view.x >= clip.x + clip.wide || y + view.y >= clip.y + clip.tall)
        {
            return hit;
        }

        render::ScopedGlState saved;
        GLint output = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &output);

        const ncl_gl::Program* pick = pick_program_.Get(shaders_, kPickProgram);
        const ncl_gl::Program* outline = outline_program_.Get(shaders_, kOutlineProgram);

        if (pick == nullptr || outline == nullptr || !EnsureTarget(view.wide, view.tall))
        {
            return hit;
        }

        if (binding_generation_ != shaders_.get_generation())
        {
            bones_block_ = pick->UniformBlockIndex("Bones");
            coverage_bindings_ = MaterialCoverageBindings::Prepare(*pick);
            binding_generation_ = shaders_.get_generation();
        }

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glViewport(0, 0, view.wide, view.tall);
        glEnable(GL_SCISSOR_TEST);
        glScissor(clip.x - view.x, clip.y - view.y, clip.wide, clip.tall);
        glDisable(GL_BLEND);
        glDisable(GL_ALPHA_TEST);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LEQUAL);
        glDepthRange(0, 1);
        glClearDepth(1);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearBufferuiv(GL_COLOR, 0, kEmptyPickId);
        glClear(GL_DEPTH_BUFFER_BIT);

        glUseProgram(pick->prog);
        glUniformBlockBinding(pick->prog, bones_block_, render::kStudioBonesBinding);
        glBindBufferBase(GL_UNIFORM_BUFFER, render::kStudioBonesBinding, bones_);
        glActiveTexture(GL_TEXTURE0 + render::kStudioBaseTextureUnit);
        glBindSampler(render::kStudioBaseTextureUnit, 0);
        glUniform1i(pick->loc[1], render::kStudioBaseTextureUnit);

        identities_.clear();
        bool camera_depth = false;
        render::StudioMaterialState material_textures;
        const std::array<int, 4> coverage_viewport = {0, 0, view.wide, view.tall};

        for (const Packet& captured : packets_)
        {
            const render::StudioDrawPacket& packet = captured.draw;
            if (!coverage_.is_texture_revision(packet.texture_revision))
            {
                continue;
            }

            if (captured.camera_space && !camera_depth)
            {
                glClear(GL_DEPTH_BUFFER_BIT);
                camera_depth = true;
            }

            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            glFrontFace(captured.mirror ? GL_CW : GL_CCW);
            glDepthRange(packet.raster.depth_range[0], packet.raster.depth_range[1]);
            glMatrixMode(GL_MODELVIEW);
            glLoadMatrixf(packet.modelview.data());
            glMatrixMode(GL_PROJECTION);
            glLoadMatrixf(packet.projection.data());
            glBindBuffer(GL_UNIFORM_BUFFER, bones_);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, packet.bone_count * 12 * sizeof(float), packet.bones.data());
            glBindVertexArray(packet.vao);
            glUniform3fv(pick->loc[5], 1, packet.chrome_origin.data());
            glUniform3fv(pick->loc[6], 1, packet.view_right.data());

            for (const render::StudioQueuedMesh& mesh : packet.meshes)
            {
                auto identity = std::find_if(identities_.begin(), identities_.end(), [&captured, &mesh](const PreviewHit& candidate) {
                    return candidate.model_index == captured.model && candidate.texture_index == mesh.skin;
                });
                GLuint id = 0;

                if (identity == identities_.end())
                {
                    identities_.push_back({captured.model, mesh.skin});
                    id = static_cast<GLuint>(identities_.size());
                }
                else
                {
                    id = static_cast<GLuint>(identity - identities_.begin()) + 1;
                }

                const ncl_material::Material* material = mesh.material ? &*mesh.material : nullptr;
                const ncl_material::ProgramManifest* manifest = mesh.images.manifest;
                coverage_.Upload(
                    coverage_bindings_,
                    material,
                    manifest,
                    coverage_viewport,
                    packet.effect_time,
                    material_textures,
                    mesh.images.coverage_dissolve_map
                );
                glBindTexture(GL_TEXTURE_2D, mesh.texture);
                glUniform2fv(pick->loc[0], 1, mesh.texel_scale);
                glUniform1i(pick->loc[2], mesh.masked ? 1 : 0);
                glUniform1ui(pick->loc[3], id);
                glUniform1i(pick->loc[4], mesh.chrome ? 1 : 0);
                const float alpha = render::StudioContract_PickingAlpha(material, manifest, packet.blend);
                glUniform1f(pick->loc[7], alpha);
                glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, reinterpret_cast<const void*>(mesh.index_offset));
                material_textures.Restore();
                glActiveTexture(GL_TEXTURE0 + render::kStudioBaseTextureUnit);
            }
        }

        GLuint selected = 0;
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &selected);

        if (selected == 0 || selected > identities_.size())
        {
            return hit;
        }

        hit = identities_[selected - 1];
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(output));
        glViewport(view.x, view.y, view.wide, view.tall);
        glScissor(clip.x, clip.y, clip.wide, clip.tall);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glUseProgram(outline->prog);
        glBindVertexArray(vao_);
        glBindTexture(GL_TEXTURE_2D, ids_);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glUniform1i(outline->loc[0], 0);
        glUniform1ui(outline->loc[1], selected);
        glUniform2f(outline->loc[2], static_cast<float>(view.x), static_cast<float>(view.y));
        glDrawArrays(GL_TRIANGLES, 0, 3);
        return hit;
    }

    void PreviewInteraction::Clear()
    {
        if (device_.IsContextCurrent())
        {
            glDeleteFramebuffers(1, &framebuffer_);
            glDeleteTextures(1, &ids_);
            glDeleteRenderbuffers(1, &depth_);
            glDeleteBuffers(1, &bones_);
            glDeleteVertexArrays(1, &vao_);
        }

        OnDeviceLost();
    }

    void PreviewInteraction::OnDeviceLost()
    {
        framebuffer_ = ids_ = depth_ = bones_ = vao_ = 0;
        wide_ = tall_ = 0;
        packets_.clear();
        mesh_storage_.Clear();
        identities_.clear();
    }

} // namespace render::preview
