#include "graphics/render/effects/StudioEffectTargets.h"

#include <algorithm>

#include <glad/glad.h>

#include "console/console.h"
#include "graphics/render/FramebufferDepth.h"
#include "graphics/render/RenderDevice.h"
#include "graphics/render/StudioBindings.h"
#include "graphics/render/StudioQueue.h"

namespace
{
    constexpr size_t kMaxRetainedPixels = 512 * 1024;
    constexpr GLenum kAttachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    constexpr GLfloat kEmptyColor[4]{};

    render::effects::Rect_t Union(const render::effects::Rect_t& a, const render::effects::Rect_t& b)
    {
        const int x = std::min(a[0], b[0]);
        const int y = std::min(a[1], b[1]);
        return {x, y, std::max(a[0] + a[2], b[0] + b[2]) - x, std::max(a[1] + a[3], b[1] + b[3]) - y};
    }

    void Texture(GLuint name, GLenum format, GLenum input, GLenum type, int wide, int tall)
    {
        glBindTexture(GL_TEXTURE_2D, name);
        glTexImage2D(GL_TEXTURE_2D, 0, format, wide, tall, 0, input, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    GLenum DepthTextureTarget(int samples)
    {
        return samples > 0 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
    }

    void DepthTexture(GLuint name, GLenum format, GLenum input, GLenum type, int wide, int tall, int samples)
    {
        if (samples > 0)
        {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, name);
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, format, wide, tall, GL_TRUE);
        }
        else
        {
            Texture(name, format, input, type, wide, tall);
        }
    }

} // namespace

render::StudioEffectTargets::StudioEffectTargets(RenderDevice& device) :
    device_(device)
{}

void render::StudioEffectTargets::Mask::Reset(const effects::Rect_t& area, bool reverse)
{
    rect_ = area;
    reversed_ = reverse;

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glDrawBuffers(3, kAttachments);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    for (int index = 0; index < 3; index++)
    {
        glClearBufferfv(GL_COLOR, index, kEmptyColor);
    }

    glClearDepth(reversed_ ? 0 : 1);
    glClear(GL_DEPTH_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, depth_framebuffer_);
    glClear(GL_DEPTH_BUFFER_BIT);
}

bool render::StudioEffectTargets::Mask::Allocate(const effects::Rect_t& area, bool reverse, int sample_count)
{
    rect_ = area;
    reversed_ = reverse;
    samples_ = sample_count;

    glGenFramebuffers(1, &framebuffer_);
    glGenFramebuffers(1, &depth_framebuffer_);
    GLuint images[5]{};
    glGenTextures(5, images);
    fill_ = images[0];
    outline_ = images[1];
    shape_ = images[2];
    mask_depth_ = images[3];
    object_depth_ = images[4];

    glActiveTexture(GL_TEXTURE0);
    for (int index = 0; index < 3; index++)
    {
        Texture(images[index], GL_RGBA16F, GL_RGBA, GL_FLOAT, rect_[2], rect_[3]);
    }

    Texture(mask_depth_, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, rect_[2], rect_[3]);
    DepthTexture(object_depth_, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, rect_[2], rect_[3], samples_);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    for (int index = 0; index < 3; index++)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, kAttachments[index], GL_TEXTURE_2D, images[index], 0);
    }
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mask_depth_, 0);
    glDrawBuffers(3, kAttachments);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, depth_framebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, DepthTextureTarget(samples_), object_depth_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        return false;
    }

    Reset(area, reverse);

    return true;
}

void render::StudioEffectTargets::Mask::Release(bool release_gl)
{
    if (release_gl)
    {
        glDeleteFramebuffers(1, &framebuffer_);
        glDeleteFramebuffers(1, &depth_framebuffer_);
        const GLuint images[] = {fill_, outline_, shape_, mask_depth_, object_depth_};
        glDeleteTextures(5, images);
    }

    framebuffer_ = depth_framebuffer_ = 0;
    fill_ = outline_ = shape_ = mask_depth_ = object_depth_ = 0;
}

render::StudioEffectTargets::Mask* render::StudioEffectTargets::AcquireMask(
    Masks_t& masks,
    uintptr_t entity,
    const effects::Rect_t& bounds,
    bool reversed,
    int samples,
    bool& reused
)
{
    reused = false;
    auto existing =
        std::find_if(masks.begin(), masks.end(), [entity](const std::unique_ptr<Mask>& target) { return target->get_entity() == entity; });
    Mask* target = existing == masks.end() ? nullptr : existing->get();
    const effects::Rect_t rect = target == nullptr ? bounds : Union(target->get_rect(), bounds);

    if (target != nullptr && rect == target->get_rect())
    {
        return target;
    }

    auto reusable = std::find_if(pool_.begin(), pool_.end(), [&rect, samples](const std::unique_ptr<Mask>& cached) {
        return cached->get_rect()[2] == rect[2] && cached->get_rect()[3] == rect[3] && cached->get_samples() == samples;
    });

    std::unique_ptr<Mask> replacement;
    if (reusable != pool_.end())
    {
        replacement = std::move(*reusable);
        pool_.erase(reusable);
        replacement->Reset(rect, reversed);
        reused = true;
    }
    else
    {
        replacement = std::make_unique<Mask>();
        if (!replacement->Allocate(rect, reversed, samples))
        {
            replacement->Release(device_.IsContextCurrent());
            Con_DPrintf(ConLogType::Warning, "render: material effect target is incomplete\n");
            return nullptr;
        }
    }

    replacement->entity_ = entity;
    if (target != nullptr)
    {
        PreserveMask(*target, *replacement);
        target->Release(device_.IsContextCurrent());
        *existing = std::move(replacement);

        return existing->get();
    }

    masks.push_back(std::move(replacement));

    return masks.back().get();
}

void render::StudioEffectTargets::PreserveMask(const Mask& source, const Mask& destination)
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, source.framebuffer_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination.framebuffer_);

    const int x = source.rect_[0] - destination.rect_[0];
    const int y = source.rect_[1] - destination.rect_[1];

    for (int index = 0; index < 3; index++)
    {
        glReadBuffer(kAttachments[index]);
        glDrawBuffer(kAttachments[index]);

        glBlitFramebuffer(
            0, 0, source.rect_[2], source.rect_[3], x, y, x + source.rect_[2], y + source.rect_[3], GL_COLOR_BUFFER_BIT, GL_NEAREST
        );
    }

    glBlitFramebuffer(
        0, 0, source.rect_[2], source.rect_[3], x, y, x + source.rect_[2], y + source.rect_[3], GL_DEPTH_BUFFER_BIT, GL_NEAREST
    );

    glBindFramebuffer(GL_READ_FRAMEBUFFER, source.depth_framebuffer_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination.depth_framebuffer_);

    glBlitFramebuffer(
        0, 0, source.rect_[2], source.rect_[3], x, y, x + source.rect_[2], y + source.rect_[3], GL_DEPTH_BUFFER_BIT, GL_NEAREST
    );
}

void render::StudioEffectTargets::RecycleMasks(Masks_t& masks)
{
    size_t retained_pixels = 0;
    for (const std::unique_ptr<Mask>& cached : pool_)
    {
        retained_pixels += static_cast<size_t>(cached->get_rect()[2]) * cached->get_rect()[3];
    }

    for (std::unique_ptr<Mask>& target : masks)
    {
        const size_t pixels = static_cast<size_t>(target->get_rect()[2]) * target->get_rect()[3];

        if (retained_pixels + pixels <= kMaxRetainedPixels)
        {
            retained_pixels += pixels;
            pool_.push_back(std::move(target));
        }
        else
        {
            target->Release(device_.IsContextCurrent());
        }
    }

    masks.clear();
}

void render::StudioEffectTargets::ReleaseMasks(Masks_t& masks, bool release_gl)
{
    for (std::unique_ptr<Mask>& target : masks)
    {
        target->Release(release_gl);
    }

    masks.clear();
}

void render::StudioEffectTargets::Clear(bool release_gl)
{
    ReleaseMasks(pool_, release_gl);

    if (release_gl)
    {
        glDeleteBuffers(1, &bones_);
        glDeleteVertexArrays(1, &vao_);
        glDeleteFramebuffers(1, &scene_framebuffer_);
        glDeleteTextures(1, &scene_depth_);
        glDeleteFramebuffers(1, &capture_depth_framebuffer_);
        glDeleteTextures(1, &capture_depth_image_);
        glDeleteFramebuffers(2, seed_framebuffers_);
        glDeleteTextures(2, seeds_);
    }

    bones_ = vao_ = scene_framebuffer_ = scene_depth_ = capture_depth_framebuffer_ = capture_depth_image_ = 0;
    capture_depth_wide_ = capture_depth_tall_ = capture_depth_samples_ = 0;
    std::fill(std::begin(seed_framebuffers_), std::end(seed_framebuffers_), 0);
    std::fill(std::begin(seeds_), std::end(seeds_), 0);
    scene_wide_ = scene_tall_ = scene_samples_ = seed_wide_ = seed_tall_ = 0;
    scene_format_ = 0;
    depth_tolerance_ = 0;
}

void render::StudioEffectTargets::EnsureGeometry()
{
    if (bones_ == 0)
    {
        glGenBuffers(1, &bones_);
        glBindBuffer(GL_UNIFORM_BUFFER, bones_);
        glBufferData(GL_UNIFORM_BUFFER, render::kStudioBoneFloats * sizeof(float), nullptr, GL_STREAM_DRAW);
        glGenVertexArrays(1, &vao_);
    }
}

bool render::StudioEffectTargets::EnsureCaptureDepth(const effects::Rect_t& viewport, int samples)
{
    const int wide = std::max(1, viewport[0] + viewport[2]);
    const int tall = std::max(1, viewport[1] + viewport[3]);

    if ((capture_depth_samples_ > 0) != (samples > 0))
    {
        glDeleteTextures(1, &capture_depth_image_);
        capture_depth_image_ = 0;
    }

    if (capture_depth_framebuffer_ == 0)
    {
        glGenFramebuffers(1, &capture_depth_framebuffer_);
    }

    if (capture_depth_image_ == 0)
    {
        glGenTextures(1, &capture_depth_image_);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, capture_depth_framebuffer_);
    if (capture_depth_wide_ != wide || capture_depth_tall_ != tall || capture_depth_samples_ != samples)
    {
        glActiveTexture(GL_TEXTURE0);
        DepthTexture(capture_depth_image_, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, wide, tall, samples);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, DepthTextureTarget(samples), capture_depth_image_, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        capture_depth_wide_ = wide;
        capture_depth_tall_ = tall;
        capture_depth_samples_ = samples;
    }

    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

bool render::StudioEffectTargets::SnapshotDepth(GLuint output, const effects::Rect_t& viewport)
{
    glBindFramebuffer(GL_FRAMEBUFFER, output);
    GLint samples = 0;
    glGetIntegerv(GL_SAMPLES, &samples);

    render::FramebufferDepthFormat_t depth;
    if (!render::Framebuffer_QueryDepthFormat(GL_FRAMEBUFFER, output, depth))
    {
        return false;
    }

    const int stencil_bits = depth.stencil_bits;
    const GLenum component = depth.floating ? GL_FLOAT : GL_UNSIGNED_NORMALIZED;

    // The mask uses D24, and both depth samples are converted to float before comparison.
    const float scene_step = depth.floating ? 1.0f / 16777216.0f : 1.0f / static_cast<float>((1u << std::min(depth.depth_bits, 24)) - 1u);
    depth_tolerance_ = 1.0f / 16777215.0f + scene_step;

    const GLenum format = depth.internal_format;

    if ((scene_samples_ > 0) != (samples > 0))
    {
        glDeleteTextures(1, &scene_depth_);
        scene_depth_ = 0;
    }

    if (scene_framebuffer_ == 0)
    {
        glGenFramebuffers(1, &scene_framebuffer_);
    }

    if (scene_depth_ == 0)
    {
        glGenTextures(1, &scene_depth_);
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, scene_framebuffer_);
    if (scene_wide_ != viewport[2] || scene_tall_ != viewport[3] || scene_format_ != format || scene_samples_ != samples)
    {
        glActiveTexture(GL_TEXTURE0);
        DepthTexture(
            scene_depth_,
            format,
            stencil_bits > 0 ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT,
            stencil_bits > 0 ? (component == GL_FLOAT ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_UNSIGNED_INT_24_8) : GL_FLOAT,
            viewport[2],
            viewport[3],
            samples
        );
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, DepthTextureTarget(samples), scene_depth_, 0);
        glDrawBuffer(GL_NONE);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_framebuffer_);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, output);

        scene_wide_ = viewport[2];
        scene_tall_ = viewport[3];
        scene_format_ = format;
        scene_samples_ = samples;
    }

    if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        return false;
    }

    glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(
        viewport[0],
        viewport[1],
        viewport[0] + viewport[2],
        viewport[1] + viewport[3],
        0,
        0,
        viewport[2],
        viewport[3],
        GL_DEPTH_BUFFER_BIT,
        GL_NEAREST
    );

    return true;
}

void render::StudioEffectTargets::EnsureSeeds(int wide, int tall)
{
    if (seeds_[0] == 0)
    {
        glGenTextures(2, seeds_);
        glGenFramebuffers(2, seed_framebuffers_);
    }

    if (wide == seed_wide_ && tall == seed_tall_)
    {
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    for (int index = 0; index < 2; index++)
    {
        Texture(seeds_[index], GL_RG32I, GL_RG_INTEGER, GL_INT, wide, tall);
        glBindFramebuffer(GL_FRAMEBUFFER, seed_framebuffers_[index]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, seeds_[index], 0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    }

    seed_wide_ = wide;
    seed_tall_ = tall;
}
