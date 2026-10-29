#include "graphics/render/effects/StudioEffectPasses.h"

#include <algorithm>

#include <glad/glad.h>

#include "graphics/render/material/MaterialCoverageRuntime.h"
#include "graphics/render/ShaderManager.h"
#include "graphics/render/StudioSubmit.h"
#include "graphics/render/material/studio_effect_material.h"

namespace
{
    constexpr GLenum kAttachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    constexpr render::ProgramDesc kMask{"preview_pick.vert", "studio_effect_mask.frag", nullptr, nullptr, 0, nullptr, 0};
    constexpr render::ProgramDesc kSeed{"preview_outline.vert", "studio_effect_seed.frag", nullptr, nullptr, 0, nullptr, 0};
    constexpr render::ProgramDesc kDistance{"preview_outline.vert", "studio_effect_distance.frag", nullptr, nullptr, 0, nullptr, 0};
    constexpr render::ProgramDesc kComposite{"preview_outline.vert", "studio_effect_composite.frag", nullptr, nullptr, 0, nullptr, 0};

    void Bind(int location, GLuint image, int unit)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindSampler(unit, 0);
        glBindTexture(GL_TEXTURE_2D, image);
        glUniform1i(location, unit);
    }

} // namespace

void render::StudioEffectPasses::BindDepth(
    const DepthBindings& bindings,
    GLuint object,
    GLuint scene,
    int samples,
    int object_unit,
    int scene_unit
)
{
    Bind(bindings.object_depth, samples > 0 ? 0 : object, object_unit);
    Bind(bindings.scene_depth, samples > 0 ? 0 : scene, scene_unit);
    for (int index = 0; index < 2; index++)
    {
        const int unit = 6 + index;
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindSampler(unit, 0);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, samples > 0 ? (index == 0 ? object : scene) : 0);
        glUniform1i(index == 0 ? bindings.object_depth_ms : bindings.scene_depth_ms, unit);
    }
    glUniform1i(bindings.samples, samples);
}

render::StudioEffectPasses::StudioEffectPasses(StudioEffectTargets& targets, ShaderManager& shaders, MaterialCoverageRuntime& coverage) :
    targets_(targets),
    shaders_(shaders),
    coverage_(coverage)
{}

const ncl_gl::Program* render::StudioEffectPasses::PrepareCapture()
{
    const ncl_gl::Program* mask = mask_.Get(shaders_, kMask);
    if (mask == nullptr)
    {
        return nullptr;
    }

    if (mask_generation_ != shaders_.get_generation())
    {
        mask_bindings_.bones = mask->UniformBlockIndex("Bones");
        mask_bindings_.coverage = MaterialCoverageBindings::Prepare(*mask);
        mask_bindings_.chrome_origin = mask->UniformLocation("uChromeOrigin");
        mask_bindings_.view_right = mask->UniformLocation("uViewRight");
        mask_bindings_.base = mask->UniformLocation("uBase");
        mask_bindings_.texel_scale = mask->UniformLocation("uTexelScale");
        mask_bindings_.chrome = mask->UniformLocation("uChrome");
        mask_bindings_.masked = mask->UniformLocation("uMasked");
        mask_bindings_.alpha_scale = mask->UniformLocation("uAlphaScale");
        mask_bindings_.fill = mask->UniformLocation("uFill");
        mask_bindings_.outline = mask->UniformLocation("uOutline");
        mask_bindings_.outline_mode = mask->UniformLocation("uOutlineMode");
        mask_bindings_.shape = mask->UniformLocation("uShape");
        mask_generation_ = shaders_.get_generation();
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DITHER);
    glDisable(GL_FRAMEBUFFER_SRGB);

    return mask;
}

void render::StudioEffectPasses::CaptureMask(
    const StudioEffectTargets::Mask& target,
    const StudioDrawPacket& packet,
    const ncl_gl::Program& mask
)
{
    const effects::Rect_t& rect = target.get_rect();
    const bool reversed = packet.raster.depth_range[0] > packet.raster.depth_range[1];

    targets_.EnsureGeometry();
    glBindFramebuffer(GL_FRAMEBUFFER, target.get_framebuffer());
    glDrawBuffers(3, kAttachments);
    glViewport(-rect[0], -rect[1], packet.viewport[2], packet.viewport[3]);
    glDepthRange(packet.raster.depth_range[0], packet.raster.depth_range[1]);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(reversed ? GL_GEQUAL : GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    Studio_ApplyFaceCulling(packet.flipped);

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(packet.modelview.data());
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(packet.projection.data());

    glUseProgram(mask.prog);
    glUniformBlockBinding(mask.prog, mask_bindings_.bones, kStudioBonesBinding);
    glBindBuffer(GL_UNIFORM_BUFFER, targets_.get_bones());
    glBufferSubData(GL_UNIFORM_BUFFER, 0, packet.bone_count * 12 * sizeof(float), packet.bones.data());
    glBindBufferBase(GL_UNIFORM_BUFFER, kStudioBonesBinding, targets_.get_bones());
    glBindVertexArray(packet.vao);
    glUniform3fv(mask_bindings_.chrome_origin, 1, packet.chrome_origin.data());
    glUniform3fv(mask_bindings_.view_right, 1, packet.view_right.data());

    DrawMeshes(packet, {-rect[0], -rect[1], packet.viewport[2], packet.viewport[3]});
}

void render::StudioEffectPasses::DrawMeshes(const StudioDrawPacket& packet, const effects::Rect_t& viewport)
{
    StudioMaterialState textures;
    glBindSampler(kStudioBaseTextureUnit, 0);
    glBindSampler(kFirstMaterialUnit, 0);

    for (const StudioQueuedMesh& mesh : packet.meshes)
    {
        const ncl_material::Material* material = mesh.material ? &*mesh.material : nullptr;
        const ncl_material::ProgramManifest* manifest = mesh.images.manifest;
        coverage_.Upload(
            mask_bindings_.coverage, material, manifest, viewport, packet.effect_time, textures, mesh.images.coverage_dissolve_map
        );
        Bind(mask_bindings_.base, mesh.texture, kStudioBaseTextureUnit);
        glUniform2fv(mask_bindings_.texel_scale, 1, mesh.texel_scale);
        glUniform1i(mask_bindings_.chrome, mesh.chrome);
        glUniform1i(mask_bindings_.masked, mesh.masked);

        const float alpha = StudioContract_PickingAlpha(material, manifest, packet.blend);
        glUniform1f(mask_bindings_.alpha_scale, alpha);
        std::array<float, 4> fill = effects::Color(material, manifest, StudioParameterRole::OccludedColor);
        std::array<float, 4> outline = effects::Color(material, manifest, StudioParameterRole::OutlineColor);
        fill[3] *= effects::Scalar(material, manifest, StudioParameterRole::OccludedAmount) * alpha;
        outline[3] *= alpha;
        glUniform4fv(mask_bindings_.fill, 1, fill.data());
        glUniform4fv(mask_bindings_.outline, 1, outline.data());

        glUniform1f(mask_bindings_.outline_mode, effects::Scalar(material, manifest, StudioParameterRole::OutlineMode));
        glUniform3f(
            mask_bindings_.shape,
            effects::Scalar(material, manifest, StudioParameterRole::OutlineWidth),
            effects::Scalar(material, manifest, StudioParameterRole::GlowRadius),
            effects::Scalar(material, manifest, StudioParameterRole::GlowStrength)
        );

        glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, reinterpret_cast<const void*>(mesh.index_offset));

        textures.Restore();
    }
}

void render::StudioEffectPasses::CaptureDepth(
    const StudioEffectTargets::Mask& target,
    const StudioDrawPacket& packet,
    const ncl_gl::Program& mask
)
{
    const effects::Rect_t& rect = target.get_rect();
    const bool reversed = packet.raster.depth_range[0] > packet.raster.depth_range[1];

    // Depth interpolation must use the main pass window coordinates even though retained masks are cropped.
    glBindFramebuffer(GL_FRAMEBUFFER, targets_.get_capture_depth_framebuffer());
    glClearDepth(reversed ? 0 : 1);
    glClear(GL_DEPTH_BUFFER_BIT);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, target.get_depth_framebuffer());
    const int x = packet.viewport[0] + rect[0];
    const int y = packet.viewport[1] + rect[1];
    glBlitFramebuffer(0, 0, rect[2], rect[3], x, y, x + rect[2], y + rect[3], GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, targets_.get_capture_depth_framebuffer());
    glViewport(packet.viewport[0], packet.viewport[1], packet.viewport[2], packet.viewport[3]);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, rect[2], rect[3]);
    DrawMeshes(packet, packet.viewport);

    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target.get_depth_framebuffer());
    glBlitFramebuffer(x, y, x + rect[2], y + rect[3], 0, 0, rect[2], rect[3], GL_DEPTH_BUFFER_BIT, GL_NEAREST);
}

bool render::StudioEffectPasses::PrepareComposition(const effects::Rect_t& viewport, Composition& composition)
{
    composition.clipped_ = glIsEnabled(GL_SCISSOR_TEST) != 0;
    glGetIntegerv(GL_SCISSOR_BOX, composition.clip_.data());
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &composition.output_);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    composition.seed_ = seed_.Get(shaders_, kSeed);
    composition.distance_ = distance_.Get(shaders_, kDistance);
    composition.composite_ = composite_.Get(shaders_, kComposite);
    if (composition.seed_ == nullptr || composition.distance_ == nullptr || composition.composite_ == nullptr ||
        !targets_.SnapshotDepth(composition.output_, viewport))
    {
        return false;
    }

    if (composition_generation_ != shaders_.get_generation())
    {
        seed_bindings_.depth = PrepareDepth(*composition.seed_);
        seed_bindings_.shape = composition.seed_->UniformLocation("uShape");
        distance_bindings_ = {composition.distance_->UniformLocation("uStep"), composition.distance_->UniformLocation("uSeeds")};
        composite_bindings_.depth = PrepareDepth(*composition.composite_);
        composite_bindings_.fill = composition.composite_->UniformLocation("uFill");
        composite_bindings_.outline = composition.composite_->UniformLocation("uOutline");
        composite_bindings_.shape = composition.composite_->UniformLocation("uShape");
        composite_bindings_.seeds = composition.composite_->UniformLocation("uSeeds");
        composite_bindings_.output_origin = composition.composite_->UniformLocation("uOutputOrigin");
        composition_generation_ = shaders_.get_generation();
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_DITHER);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindVertexArray(targets_.get_vertex_array());

    return true;
}

void render::StudioEffectPasses::Seed(
    const StudioEffectTargets::Mask& target,
    const effects::Rect_t& viewport,
    const Composition& composition
)
{
    const effects::Rect_t& rect = target.get_rect();

    targets_.EnsureSeeds(viewport[2], viewport[3]);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, targets_.get_seed_framebuffer(0));
    glViewport(0, 0, viewport[2], viewport[3]);

    glUseProgram(composition.seed_->prog);
    Bind(seed_bindings_.shape, target.get_shape(), 0);
    BindDepth(seed_bindings_.depth, target.get_object_depth(), targets_.get_scene_depth(), target.get_samples(), 1, 2);
    glUniform2i(seed_bindings_.depth.origin, rect[0], rect[1]);
    glUniform1f(seed_bindings_.depth.sign, target.is_reversed() ? -1.0f : 1.0f);
    glUniform1f(seed_bindings_.depth.tolerance, targets_.get_depth_tolerance());

    glDrawArrays(GL_TRIANGLES, 0, 3);
}

int render::StudioEffectPasses::PropagateSeeds(const effects::Rect_t& viewport, const Composition& composition)
{
    const GLint step_location = distance_bindings_.step;
    glUseProgram(composition.distance_->prog);

    int current = 0;
    int step = 1;

    while (step < std::max(viewport[2], viewport[3]))
    {
        step *= 2;
    }

    for (step /= 2; step >= 1; step /= 2)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, targets_.get_seed_framebuffer(1 - current));
        Bind(distance_bindings_.seeds, targets_.get_seed_image(current), 0);
        glUniform1i(step_location, step);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        current = 1 - current;
    }

    return current;
}

void render::StudioEffectPasses::Composite(
    const StudioEffectTargets::Mask& target,
    const effects::Rect_t& viewport,
    int seed_index,
    const Composition& composition
)
{
    const effects::Rect_t& rect = target.get_rect();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, composition.output_);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    glUseProgram(composition.composite_->prog);
    Bind(composite_bindings_.fill, target.get_fill(), 0);
    Bind(composite_bindings_.outline, target.get_outline(), 1);
    Bind(composite_bindings_.shape, target.get_shape(), 2);
    BindDepth(composite_bindings_.depth, target.get_object_depth(), targets_.get_scene_depth(), target.get_samples(), 3, 4);
    Bind(composite_bindings_.seeds, targets_.get_seed_image(seed_index), 5);
    glUniform2i(composite_bindings_.depth.origin, rect[0], rect[1]);
    glUniform2i(composite_bindings_.output_origin, viewport[0], viewport[1]);
    glUniform1f(composite_bindings_.depth.sign, target.is_reversed() ? -1.0f : 1.0f);
    glUniform1f(composite_bindings_.depth.tolerance, targets_.get_depth_tolerance());

    if (composition.clipped_)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(composition.clip_[0], composition.clip_[1], composition.clip_[2], composition.clip_[3]);
    }

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

render::StudioEffectPasses::DepthBindings render::StudioEffectPasses::PrepareDepth(const ncl_gl::Program& program)
{
    return {
        program.UniformLocation("uObjectDepth"),
        program.UniformLocation("uSceneDepth"),
        program.UniformLocation("uObjectDepthMs"),
        program.UniformLocation("uSceneDepthMs"),
        program.UniformLocation("uSamples"),
        program.UniformLocation("uOrigin"),
        program.UniformLocation("uDepthSign"),
        program.UniformLocation("uDepthTolerance")
    };
}
