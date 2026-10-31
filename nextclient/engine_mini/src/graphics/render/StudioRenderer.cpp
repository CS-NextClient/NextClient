#include "graphics/render/StudioRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

#include <glad/glad.h>
#include <goldsrc/efx_api.h>
#include <goldsrc/format/studio.h>
#include <goldsrc/studio_api.h>
#include <ncl_math/vec3.h>
#include <ncl_studio/StudioMesh.h>
#include <tier1/strtools.h>

#include "graphics/preview/PreviewInteraction.h"
#include "graphics/render/ScopedGlState.h"
#include "graphics/render/StudioDiagnostics.h"
#include "graphics/render/StudioDump.h"
#include "graphics/render/StudioIntercept.h"
#include "graphics/render/StudioPreviewContext.h"
#include "graphics/render/StudioSubmit.h"
#include "graphics/render/effects/StudioEffects.h"
#include "graphics/render/material/MaterialCoverageRuntime.h"
#include "graphics/render/material/MaterialEditSession.h"
#include "graphics/render/material/ServerMaterials.h"
#include "graphics/render/model/StudioGeometry.h"
#include "graphics/render/model/studio_sub_model.h"
#include "graphics/render/render_context.h"
#include "performance/PerformanceMonitor.h"
#include "performance/performance_scopes.h"

namespace render
{
    namespace
    {
        constexpr size_t kBoneMatrixFloats = 3 * 4;

        const ncl_studio::SubModelRange* FindByOffset(const ncl_studio::StudioMesh& mesh, uint32_t file_offset)
        {
            for (const ncl_studio::SubModelRange& range : mesh.submodels)
            {
                if (range.file_offset == file_offset)
                {
                    return &range;
                }
            }

            return nullptr;
        }

        const ncl_studio::SubModelRange* ResolveSubModel(void* raw, const goldsrc::studiohdr_t* header, const ncl_studio::StudioMesh& mesh)
        {
            const goldsrc::mstudiomodel_t* submodel = Studio_ResolveSubModel(raw, header);

            if (!submodel)
            {
                return nullptr;
            }

            uintptr_t offset = reinterpret_cast<uintptr_t>(submodel) - reinterpret_cast<uintptr_t>(header);

            return FindByOffset(mesh, uint32_t(offset));
        }

        // Textures may live in the model itself or in a companion <name>T.mdl, and the skin
        // table lives in whichever of the two holds them, not in the model header.
        struct TextureSource
        {
            const goldsrc::studiohdr_t* header{};
            goldsrc::model_t* model{};
        };

        TextureSource ResolveTextureHeader(
            const goldsrc::engine_studio_api_t* api,
            goldsrc::model_t* model,
            const goldsrc::studiohdr_t* header
        )
        {
            if (header->textureindex != 0)
            {
                return {header, model};
            }

            if (api->Mod_ForName == nullptr || api->Mod_Extradata == nullptr)
            {
                return {};
            }

            char name[64 + 2];
            size_t length = V_strlen(model->name);

            if (length < 4 || length + 2 > sizeof(name))
            {
                return {};
            }

            V_strcpy_safe(name, model->name);
            V_strcpy(name + length - 4, "T.mdl");

            goldsrc::model_t* texture_model = api->Mod_ForName(name, 0);

            if (texture_model == nullptr)
            {
                return {};
            }

            return {static_cast<const goldsrc::studiohdr_t*>(api->Mod_Extradata(texture_model)), texture_model};
        }

        int ActiveSkinFamily(const goldsrc::cl_entity_t* entity, const goldsrc::studiohdr_t* texture_header)
        {
            int skin = entity != nullptr ? entity->curstate.skin : 0;
            return skin > 0 && skin < texture_header->numskinfamilies ? skin : 0;
        }

        const goldsrc::mstudiotexture_t* ResolveTexture(const goldsrc::studiohdr_t* texture_header, int skin_family, int32_t skinref)
        {
            if (skinref < 0 || skinref >= texture_header->numskinref)
            {
                return nullptr;
            }

            const short* skins =
                reinterpret_cast<const short*>(reinterpret_cast<const uint8_t*>(texture_header) + texture_header->skinindex) +
                static_cast<size_t>(texture_header->numskinref) * skin_family;

            int texture_index = skins[skinref];

            if (texture_index < 0 || texture_index >= texture_header->numtextures)
            {
                return nullptr;
            }

            const goldsrc::mstudiotexture_t* textures = reinterpret_cast<const goldsrc::mstudiotexture_t*>(
                reinterpret_cast<const uint8_t*>(texture_header) + texture_header->textureindex
            );

            return &textures[texture_index];
        }

        // kShadeFullbright is a deliberate addition: the stock engine only declares
        // STUDIO_NF_FULLBRIGHT and never reads it, so a model authored with it is currently lit
        // like any other. Honoring it makes the shader path differ from the original on
        // purpose, and the A/B comparison will differ for such models.
        int ShadeModeFor(int flags, bool glow_shell, int rendermode)
        {
            // The shell is neither lit nor blended: the engine's forced chrome branch emits no
            // vertex color at all, so every vertex of it keeps the flat color the pass was
            // opened with, which is the entity's rendercolor.
            if (glow_shell)
            {
                return kShadeGlowShell;
            }

            // Additive entities are not lit at all: the engine replaces every light value with
            // r_blend rather than running the lighting formula, so the model reads as its own
            // texture at a uniform brightness. This outranks the texture's own flags because
            // the engine decides it before it looks at them.
            if (rendermode == goldsrc::kRenderTransAdd)
            {
                return kShadeAdditive;
            }

            if ((flags & goldsrc::STUDIO_NF_FULLBRIGHT) != 0)
            {
                return kShadeFullbright;
            }

            return (flags & goldsrc::STUDIO_NF_FLATSHADE) != 0 ? kShadeFlat : kShadeLit;
        }

        const char* UnsupportedReason(const goldsrc::cl_entity_t* entity, int forced_face_flags)
        {
            // Chrome is the only flag anything forces, and the shape of that pass is read off
            // it: no skin bound, a flat color, a sphere map driven from an orbiting point. A
            // different forced flag would keep the engine's ordinary lighting under the
            // additive render mode the forcing selects, which is not reproduced here.
            if ((forced_face_flags & ~goldsrc::STUDIO_NF_CHROME) != 0)
            {
                return "forced face flags other than chrome are set";
            }

            // What the shell is drawn with beyond the flag itself - its color, how far the
            // vertices move out - is read off the entity, so a pass without one is a pass this
            // cannot set up.
            if (forced_face_flags != 0 && entity == nullptr)
            {
                return "forced face flags with no current entity";
            }

            return nullptr;
        }

        // Forced face flags make the complete model additive, including glow shells.
        int EffectiveRenderMode(const goldsrc::cl_entity_t* entity, int forced_face_flags)
        {
            if (forced_face_flags != 0)
            {
                return goldsrc::kRenderTransAdd;
            }

            return entity != nullptr ? entity->curstate.rendermode : goldsrc::kRenderNormal;
        }

        // Vertices move along their own normals before the bone transform, by renderamt
        // scaled the way the engine scales it. The engine reaches the normal through a vertex
        // to normal index table it rebuilds per submodel; the converted mesh already carries a
        // normal per rendered vertex, which is the same normal and needs no table.
        float GlowShellScale(const goldsrc::cl_entity_t* entity)
        {
            if (entity == nullptr || entity->curstate.renderfx != goldsrc::kRenderFxGlowShell)
            {
                return 0.0f;
            }

            return static_cast<float>(entity->curstate.renderamt) * 0.05f;
        }

        // Three, which is what the engine keeps room for.
        constexpr int kEntityLights = 3;

        // Below this a light is dropped rather than kept as the weakest of three.
        constexpr float kEntityLightFloor = 0.004f;

        ncl_math::Vector3 ShellColor(const goldsrc::cl_entity_t* entity)
        {
            return ncl_math::Vector3{
                static_cast<float>(entity->curstate.rendercolor.r) / 255.0f,
                static_cast<float>(entity->curstate.rendercolor.g) / 255.0f,
                static_cast<float>(entity->curstate.rendercolor.b) / 255.0f
            };
        }

        constexpr float kShellChromeRadius = 4000.0f;
        constexpr float kShellChromeZRate = 0.33f;

        constexpr performance::PerfCounterId kStudioFlushCounters[] = {
            performance::PerfCounterId::StudioFlushBrush,
            performance::PerfCounterId::StudioFlushFallback,
            performance::PerfCounterId::StudioFlushClient,
            performance::PerfCounterId::StudioFlushStageEnd,
            performance::PerfCounterId::StudioFlushCapacity,
            performance::PerfCounterId::StudioFlushResource,
            performance::PerfCounterId::StudioFlushImmediate,
            performance::PerfCounterId::StudioFlushMutableSkin
        };

        constexpr performance::PerfCounterId kStudioImmediateCounters[] = {
            performance::PerfCounterId::StudioImmediateOutsideOpaque,
            performance::PerfCounterId::StudioImmediateQueueDisabled,
            performance::PerfCounterId::StudioImmediateCallback,
            performance::PerfCounterId::StudioImmediateMutableSkin
        };

        static_assert(std::size(kStudioFlushCounters) == static_cast<size_t>(StudioFlushReason::Count));
        static_assert(std::size(kStudioImmediateCounters) == static_cast<size_t>(StudioImmediateReason::Count));

    } // namespace

    // Scaling destroys orthonormality; the shader receives the same scale for inverse lighting.
    const float* StudioRenderer::ScaleBones(const float* bones, int count, const float* origin, float scale)
    {
        std::memcpy(scaled_bones_, bones, sizeof(scaled_bones_));

        int limit = std::min(count, static_cast<int>(goldsrc::MAXSTUDIOBONES));

        for (int bone = 0; bone < limit; bone++)
        {
            const float* source = bones + bone * kBoneMatrixFloats;
            float* target = scaled_bones_ + bone * kBoneMatrixFloats;

            for (int row = 0; row < 3; row++)
            {
                target[row * 4 + 0] = source[row * 4 + 0] * scale;
                target[row * 4 + 1] = source[row * 4 + 1] * scale;
                target[row * 4 + 2] = source[row * 4 + 2] * scale;
                target[row * 4 + 3] = origin[row] + (source[row * 4 + 3] - origin[row]) * scale;
            }
        }

        return scaled_bones_;
    }

    bool StudioRenderer::Reject(const char* reason)
    {
        last_rejection_ = reason;

        return false;
    }

    bool StudioRenderer::RejectSubModel(const goldsrc::studiohdr_t* header, void* submodel, const ncl_studio::StudioMesh& mesh)
    {
        ptrdiff_t direct = static_cast<const uint8_t*>(submodel) - reinterpret_cast<const uint8_t*>(header);
        ptrdiff_t indirect = *static_cast<const uint8_t* const*>(submodel) - reinterpret_cast<const uint8_t*>(header);

        int written = V_snprintf(
            rejection_detail_,
            sizeof(rejection_detail_),
            "submodel offsets from header %p: direct %d, indirect %d; converted:",
            static_cast<const void*>(header),
            static_cast<int>(direct),
            static_cast<int>(indirect)
        );

        for (size_t i = 0; i < mesh.submodels.size() && written > 0 && written < static_cast<int>(sizeof(rejection_detail_)); i++)
        {
            written += V_snprintf(rejection_detail_ + written, sizeof(rejection_detail_) - written, " %u", mesh.submodels[i].file_offset);
        }

        last_rejection_ = rejection_detail_;

        return false;
    }

    bool StudioRenderer::CheckAttachmentEffects(
        const goldsrc::engine_studio_api_t* api,
        const goldsrc::cl_entity_t* entity,
        const goldsrc::model_t* parent
    )
    {
        if (engine_.is_network_session())
        {
            return false;
        }
        if (entity == nullptr || !entity->player || entity->curstate.weaponmodel <= 0 || api->GetModelByIndex == nullptr)
        {
            return false;
        }
        goldsrc::model_t* attachment = api->GetModelByIndex(entity->curstate.weaponmodel);
        if (attachment == nullptr || attachment == parent)
        {
            return false;
        }
        const goldsrc::studiohdr_t* header = static_cast<const goldsrc::studiohdr_t*>(api->Mod_Extradata(attachment));
        if (header == nullptr)
        {
            return false;
        }
        TextureSource textures_source = ResolveTextureHeader(api, attachment, header);
        const goldsrc::studiohdr_t* texture_header = textures_source.header;

        if (texture_header == nullptr ||
            !models_.PrepareMetadata(attachment, header, texture_header, textures_source.model, get_resource_barrier()))
        {
            return false;
        }
        const goldsrc::mstudiotexture_t* textures = reinterpret_cast<const goldsrc::mstudiotexture_t*>(
            reinterpret_cast<const uint8_t*>(texture_header) + texture_header->textureindex
        );
        for (int index = 0; index < texture_header->numtextures; index++)
        {
            const ncl_material::Material* material =
                !engine_.is_network_session() ? edits_.FindOverride(attachment->name, textures[index].name) : nullptr;
            if (material == nullptr)
            {
                material =
                    models_.ResolveMaterial(attachment, header, texture_header, index, textures_source.model, get_resource_barrier());
            }
            if (material != nullptr && effects_.CheckEnabled(*material))
            {
                return true;
            }
        }
        return false;
    }

    bool StudioRenderer::IsFlippedViewModel(const goldsrc::cl_entity_t* entity, const StudioViewInputs& view)
    {
        return PreviewInputs(entity) == nullptr && view.right_hand > 0.0f && entity != nullptr && entity->index == view.view_entity;
    }

    void StudioRenderer::SelectEntityLights(const goldsrc::cl_entity_t* entity, const StudioViewInputs& view, EntityLights& out)
    {
        out.count = 0;

        if (entity == nullptr || engine_.lights == nullptr)
        {
            return;
        }

        float strengths[kEntityLights] = {};

        for (int i = 0; i < static_cast<int>(engine_.light_count); i++)
        {
            const goldsrc::dlight_t& light = engine_.lights[i];

            if (!(light.die > view.time && light.radius > 0.0f))
            {
                continue;
            }

            float distance2 = ncl_math::DistanceSqr(ncl_math::Vector3::Ref(entity->origin), ncl_math::Vector3::Ref(light.origin));

            float radius2 = light.radius * light.radius;
            float strength = distance2 <= radius2 ? 1.0f : radius2 / distance2;

            if (strength <= kEntityLightFloor)
            {
                continue;
            }

            int slot = -1;

            if (out.count < kEntityLights)
            {
                slot = out.count++;
            }
            else
            {
                int weakest = 0;

                for (int j = 1; j < kEntityLights; j++)
                {
                    if (strengths[j] < strengths[weakest])
                    {
                        weakest = j;
                    }
                }

                slot = strengths[weakest] < strength ? weakest : -1;
            }

            if (slot < 0)
            {
                continue;
            }

            strengths[slot] = strength;

            for (int c = 0; c < 3; c++)
            {
                out.light[slot][c] = light.origin[c];
            }

            out.light[slot][3] = radius2;

            // The engine takes the color through the linear table before it adds anything,
            // and adds in that domain; the table is ours, so this half is exact.
            out.color[slot][0] = static_cast<float>(engine_.linear_gamma[kStudioGammaTableStride * light.color.r]);
            out.color[slot][1] = static_cast<float>(engine_.linear_gamma[kStudioGammaTableStride * light.color.g]);
            out.color[slot][2] = static_cast<float>(engine_.linear_gamma[kStudioGammaTableStride * light.color.b]);
        }
    }

    ncl_math::Vector2 StudioRenderer::GammaExponents(const StudioViewInputs& view)
    {
        float gamma = view.gamma;

        return ncl_math::Vector2{gamma, 1.0f / gamma};
    }

    ncl_math::Vector3 StudioRenderer::ShellChromeOrigin(const StudioViewInputs& view)
    {
        double phase = view.shell_frequency * view.time;

        return ncl_math::Vector3{
            static_cast<float>(std::cos(phase)) * kShellChromeRadius,
            static_cast<float>(std::sin(phase)) * kShellChromeRadius,
            static_cast<float>(std::cos(phase * kShellChromeZRate)) * kShellChromeRadius
        };
    }

    void StudioRenderer::NoteQueueFlush(void* user, StudioFlushReason reason, size_t packets)
    {
        performance::reporting::StudioFlush(
            static_cast<StudioRenderer*>(user)->monitor_, kStudioFlushCounters[static_cast<size_t>(reason)], packets
        );
    }

    void StudioRenderer::NoteImmediate(void* user, StudioImmediateReason reason)
    {
        performance::reporting::StudioImmediate(
            static_cast<StudioRenderer*>(user)->monitor_, kStudioImmediateCounters[static_cast<size_t>(reason)]
        );
    }

    void StudioRenderer::NoteProgramBind(void* user)
    {
        static_cast<StudioRenderer*>(user)->queue_.NoteProgramBind();
        static_cast<StudioRenderer*>(user)->monitor_.AddCounter(performance::PerfCounterId::StudioProgramBinds, 1);
    }

    void StudioRenderer::FlushPending()
    {
        if (!pending_.active)
        {
            return;
        }
        pending_.meshes.resize(pending_.mesh_count);
        PendingModel completed = std::move(pending_);
        pending_ = PendingModel{};
        if (!device_.is_available())
        {
            return;
        }

        const tex::TextureLoadGuard guard{
            device_.get_context_identity(),
            &preparation_epoch_,
            preparation_epoch_,
            &shaders_.get_generation_source(),
            shaders_.get_generation(),
            &textures_.get_revision_source(),
            textures_.get_revision()
        };
        for (StudioQueuedMesh& mesh : completed.meshes)
        {
            const ncl_material::Material* material = mesh.material ? &*mesh.material : nullptr;
            if (material != nullptr && !material_runtime_.PrepareImages(*mesh.material_bindings, *material, mesh.images, guard))
            {
                ++cancellation_epoch_;
                return;
            }
            const ncl_material::ProgramManifest* manifest = material != nullptr ? mesh.material_bindings->manifest : nullptr;
            if (!coverage_.PrepareImages(material, manifest, mesh.images, guard))
            {
                ++cancellation_epoch_;
                return;
            }
        }
        if (!guard.is_epoch_current() || !device_.is_context_alive())
        {
            ++cancellation_epoch_;
            return;
        }
        completed.texture_revision = textures_.get_revision();
        diagnostics_.get_phase_capture().NotePacket();
        CapturePreviewPacket(completed.entity, completed);
        const uintptr_t identity = completed.effect_identity;
        if (effects_.CheckEnabled(completed) || effects_.Contains(identity) || completed.include_occluders)
        {
            effects_.Capture(identity, completed, effects_.ProjectBounds(*completed.effect_model, completed), completed.include_occluders);
        }
        queue_.Submit(std::move(static_cast<StudioDrawPacket&>(completed)), completed.allow_deferred);
    }

    bool StudioRenderer::RejectSubmission(const char* reason, size_t queued_before, bool counted)
    {
        if (pending_.active)
        {
            pending_.mesh_count = queued_before;

            if (counted)
            {
                pending_.seen_parts--;
            }

            FlushPending();
        }

        abandoned_ = true;
        abandoned_serial_ = model_draw_.serial;

        return Reject(reason);
    }

    void StudioRenderer::BeginOpaquePass(bool enabled)
    {
        FlushPending();
        queue_.BeginOpaquePass(enabled);
    }

    void StudioRenderer::EndOpaquePass()
    {
        FlushPending();
        queue_.EndOpaquePass();
        performance::reporting::StudioQueuePeak(monitor_, queue_.stats().max_run);
    }

    void StudioRenderer::BeginTransparentPass(bool enabled)
    {
        FlushPending();
        queue_.BeginTransparentPass(enabled);
    }

    void StudioRenderer::EndTransparentPass()
    {
        FlushPending();
        queue_.EndTransparentPass();
    }

    void StudioRenderer::EndModel()
    {
        FlushPending();
    }

    void StudioRenderer::Flush(StudioFlushReason reason)
    {
        FlushPending();
        queue_.Flush(reason);
    }

    const StudioQueueStats& StudioRenderer::get_queue_stats() const
    {
        return queue_.stats();
    }

    void StudioRenderer::DiscardPending()
    {
        ++preparation_epoch_;
        ++cancellation_epoch_;
        effect_player_ = nullptr;
        attachment_effects_ = false;
        pending_ = PendingModel{};
        model_draw_ = ModelDrawCache{};
        abandoned_ = false;
        queue_.Reset();
        poses_.Reset();
    }

    void StudioRenderer::InvalidateContextReferences()
    {
        diagnostics_.get_phase_capture().SessionBoundary("context_invalidated");
        ++preparation_epoch_;
        ++cancellation_epoch_;
        material_runtime_.InvalidatePrograms();
        light_gamma_ready_ = false;
    }

    void StudioRenderer::ReleaseContextResources()
    {
        submission_sink_.Release();
        StudioRenderer::InvalidateContextReferences();
    }

    void StudioRenderer::ForgetDrawModel(const goldsrc::model_t* model)
    {
        if (pending_.model == model || model_draw_.model == model || model_draw_.texture_model == model)
        {
            ++preparation_epoch_;
            ++cancellation_epoch_;
        }
        if (pending_.model == model)
        {
            pending_ = PendingModel{};
        }

        if (model_draw_.model == model || model_draw_.texture_model == model)
        {
            model_draw_ = ModelDrawCache{};
            abandoned_ = false;
        }
    }

    // The submission proper. Wrapped below rather than called directly, so the record
    // r_studio_dump takes is written at one exit instead of at each of the rejections.

    bool StudioRenderer::TryDrawSubmission(const StudioModelInputs& inputs)
    {
        goldsrc::model_t* model = inputs.model;
        const goldsrc::studiohdr_t* header = inputs.header;
        void* submodel = inputs.submodel;

        if (inputs.serial != inputs.live_serial)
        {
            return Reject("studio model was invalidated before preparation");
        }

        // One load decides whether the record is worth filling, which it is for one frame of
        // the process's life; the submission runs hundreds of times per frame.
        const bool recording = diagnostics_.is_armed();

        if (recording)
        {
            submission_ = StudioSubmission{};
        }

        const StudioPreviewInputs* preview = PreviewInputs(inputs.entity);

        if ((preview == nullptr || !preview->authoring) &&
            (engine_.config == nullptr || engine_.config->studio_shader == nullptr || engine_.config->studio_shader->value == 0.0f))
        {
            return Reject("cvar is off");
        }

        if (!device_.CheckAvailable())
        {
            return Reject("shader pipeline unavailable, needs GL 3.3");
        }

        if (model == nullptr || header == nullptr)
        {
            return Reject("no model or header captured");
        }

        if (submodel == nullptr)
        {
            return Reject("no submodel captured, StudioSetupModel was not seen");
        }

        // StudioSetupSkin checked as early as the bone transforms: the flush cannot bind
        // without it, and finding that out with meshes already queued would draw them with
        // whatever texture was left bound instead of falling back to the engine.
        const goldsrc::engine_studio_api_t* api = inputs.api;

        if (api == nullptr || api->StudioGetBoneTransform == nullptr || api->StudioSetupSkin == nullptr)
        {
            return Reject("missing studio api entry points");
        }

        // The model and the header arrive through two independent entry points, and for a
        // player's attached weapon the intercept has to tie them together itself. Everything
        // downstream keys on the pair - the geometry cache, the material document beside the
        // model, the server table lookup - so one that could not be tied together has to go
        // back rather than be cached and looked up under the wrong name.
        if (api->Mod_Extradata == nullptr || api->Mod_Extradata(model) != header)
        {
            return Reject("the model and the studio header do not belong together");
        }

        if (inputs.serial != inputs.live_serial)
        {
            return Reject("studio model was invalidated during model lookup");
        }

        const uint32_t serial = inputs.serial;
        const goldsrc::cl_entity_t* entity = inputs.entity;
        preview = PreviewInputs(entity);

        if (recording)
        {
            submission_.entity_index = entity != nullptr ? entity->index : -1;
            submission_.view_entity = entity != nullptr && api->GetViewEntity != nullptr && entity == api->GetViewEntity();
        }

        if (abandoned_ && abandoned_serial_ == serial)
        {
            return Reject("an earlier body part of this model went back to the engine");
        }

        // The entity is part of the key as well: reusing bone transforms across entities
        // would skin the second one with the first one's pose.
        if (model_draw_.serial != serial || model_draw_.header != header || model_draw_.entity != entity)
        {
            abandoned_ = false;
            FlushPending();

            TextureSource textures_source = ResolveTextureHeader(api, model, header);

            if (serial != inputs.live_serial)
            {
                return Reject("studio model was invalidated during texture preparation");
            }

            model_draw_ = ModelDrawCache{};
            model_draw_.serial = serial;
            model_draw_.model = model;
            model_draw_.texture_model = textures_source.model;
            model_draw_.header = header;
            model_draw_.entity = entity;
            model_draw_.texture_header = textures_source.header;
            model_draw_.bones_captured = false;
            model_draw_.stable_skins = true;

            if (model_draw_.texture_header != nullptr &&
                models_.PrepareMetadata(model, header, model_draw_.texture_header, model_draw_.texture_model, get_resource_barrier()))
            {
                if (serial != inputs.live_serial)
                {
                    return Reject("studio model was invalidated during metadata preparation");
                }

                const goldsrc::studiohdr_t* textures_header = model_draw_.texture_header;
                const goldsrc::mstudiotexture_t* textures = reinterpret_cast<const goldsrc::mstudiotexture_t*>(
                    reinterpret_cast<const uint8_t*>(textures_header) + textures_header->textureindex
                );

                for (int i = 0; i < textures_header->numtextures; i++)
                {
                    if (V_strnicmp(textures[i].name, "Remap", 5) == 0 || V_stricmp(textures[i].name, "DM_Base.bmp") == 0)
                    {
                        // StudioSetupSkin may replace these engine-owned textures for new colors.
                        model_draw_.stable_skins = false;
                        break;
                    }
                }
            }

            if (entity != nullptr)
            {
                server_materials_.NoteEntity(entity->index, model != nullptr ? model->name : "");
            }
        }

        if (serial != inputs.live_serial)
        {
            return Reject("studio model was invalidated during metadata preparation");
        }

        const goldsrc::studiohdr_t* texture_header = model_draw_.texture_header;

        if (texture_header == nullptr || texture_header->textureindex == 0)
        {
            return Reject("companion T.mdl with the textures is not loaded");
        }

        if (!models_.PrepareMetadata(model, header, texture_header, model_draw_.texture_model, get_resource_barrier()))
        {
            return Reject("invalid studio metadata");
        }

        if (serial != inputs.live_serial)
        {
            return Reject("studio model was invalidated during metadata preparation");
        }

        const StudioViewInputs view = engine_.CaptureView();

        // The engine ORs these into every mesh's flags rather than treating them apart, so
        // they have to join the flags before anything reads them.
        int forced_face_flags = api->GetForceFaceFlags != nullptr ? api->GetForceFaceFlags() : 0;

        if (const char* unsupported = UnsupportedReason(entity, forced_face_flags))
        {
            return Reject(unsupported);
        }

        // The second pass of a glow shell, where the client forces chrome onto every mesh. The
        // whole pass differs from an ordinary draw, so it is decided once here rather than
        // read out of the flags again at each point that turns on it.
        bool glow_shell = (forced_face_flags & goldsrc::STUDIO_NF_CHROME) != 0;

        // A material restyles the model's own skin, and the shell pass does not draw the skin:
        // it draws the shell sprite over the silhouette, which is not the material's subject.
        bool materials_enabled =
            !glow_shell && (preview != nullptr && preview->authoring
                                ? preview->materials_enabled
                                : engine_.config->studio_materials == nullptr || engine_.config->studio_materials->value != 0.0f);

        int rendermode = EffectiveRenderMode(entity, forced_face_flags);

        if (recording)
        {
            submission_.forced_face_flags = forced_face_flags;
            submission_.renderfx = entity != nullptr ? entity->curstate.renderfx : 0;
            submission_.rendermode = entity != nullptr ? entity->curstate.rendermode : 0;
            submission_.effective_rendermode = rendermode;
            submission_.blend = view.blend;
            submission_.glow_shell = glow_shell;
            submission_.glow_scale = GlowShellScale(entity);

            if (glow_shell)
            {
                submission_.shell_color = ShellColor(entity);
            }
        }

        int skin_family = ActiveSkinFamily(entity, texture_header);

        const StudioLighting& lighting = inputs.lighting;

        if (!lighting.valid)
        {
            return Reject("no lighting captured, StudioSetupLighting was not seen");
        }

        const bool include_occluders =
            materials_enabled &&
            ((entity != nullptr &&
              server_materials_.AnyBindingMatches(entity->index, MaterialHasEffects, engine_.is_network_session(), &effects_)) ||
             (preview == nullptr && entity == effect_player_ && attachment_effects_));

        const StudioGpuModel* gpu = models_.PrepareGeometry(model, header, get_resource_barrier());

        if (gpu == nullptr)
        {
            return Reject("geometry conversion or upload failed");
        }

        if (serial != inputs.live_serial)
        {
            return Reject("studio model was invalidated during geometry preparation");
        }

        const ncl_studio::SubModelRange* range = ResolveSubModel(submodel, header, *gpu->mesh);

        if (range == nullptr)
        {
            // Nothing of this body part is queued yet, but earlier ones may be.
            RejectSubmission("", pending_.mesh_count, false);

            return RejectSubModel(header, submodel, *gpu->mesh);
        }

        if (recording)
        {
            submission_.submodel_offset = range->file_offset;
        }

        if (!light_gamma_ready_)
        {
            OnGammaChanged(engine_.CaptureLightGamma());
        }

        const ncl_gl::Program* program = material_runtime_.ProgramFor(nullptr);

        if (program == nullptr)
        {
            return Reject("studio program failed to build");
        }

        if (!submission_sink_.EnsureBones())
        {
            return Reject("bone uniform buffer could not be created");
        }

        if (!model_draw_.bones_captured)
        {
            performance::LocalCpuScope preparation(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioDataPrepare);
            const float* bones = reinterpret_cast<const float*>(api->StudioGetBoneTransform());

            if (bones == nullptr)
            {
                return Reject("bone transforms unavailable");
            }

            float scale = (preview != nullptr ? preview->scale : 1.0f);

            if (scale != 1.0f)
            {
                bones = ScaleBones(bones, header->numbones, entity->origin, scale);
            }

            model_draw_.bones = poses_.Capture(bones);

            model_draw_.bones_captured = true;
        }

        last_rejection_ = "none";

        if (!pending_.active)
        {
            {
                performance::LocalCpuScope cpu(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioDataPrepare);
                pending_.meshes = queue_.AcquireMeshStorage();
                pending_.mesh_count = 0;
                pending_.active = true;
                pending_.model = model;
                pending_.entity = entity;
                pending_.allow_deferred = model_draw_.stable_skins;
                pending_.bones = model_draw_.bones;
                pending_.bone_count = header->numbones;
            }

            Studio_CaptureRasterState(pending_, monitor_);

            {
                performance::LocalCpuScope preparation(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioDataPrepare);
                ncl_math::Vector2 gamma = GammaExponents(view);
                pending_.gamma[0] = gamma.x;
                pending_.gamma[1] = gamma.y;
                pending_.seen_parts = 0;
                pending_.expected_parts = header->numbodyparts;
                pending_.vao = gpu->vao;

                if (gpu->rigid_vao != 0 && gpu->rigid.count != 0 && range->mesh_count == 1 && submission_sink_.EnsureInstances())
                {
                    const uint32_t range_index = static_cast<uint32_t>(range - gpu->mesh->submodels.data());

                    pending_.rigid_vao = gpu->rigid_vao;
                    pending_.rigid_range = gpu->rigid.ranges[range_index];
                    pending_.rigid_index_count = gpu->rigid.index_count;
                    pending_.rigid_bindings = material_runtime_.PrepareRigidBindings();
                }

                pending_.effect_model = gpu;
                pending_.effect_identity = preview != nullptr ? preview->draw_identity : reinterpret_cast<uintptr_t>(entity);

                pending_.include_occluders = include_occluders;
                pending_.lighting = lighting;
                pending_.flipped = IsFlippedViewModel(entity, view);
                pending_.blend = view.blend;
                pending_.glow_scale = GlowShellScale(entity);
                pending_.bone_scale = (preview != nullptr ? preview->scale : 1.0f);
                pending_.effect_time = static_cast<float>(preview != nullptr ? preview->time : view.time);

                EntityLights lights{};
                if (preview == nullptr)
                {
                    SelectEntityLights(entity, view, lights);
                }
                pending_.light_count = lights.count;
                std::memcpy(pending_.lights, lights.light, sizeof(lights.light));
                std::memcpy(pending_.light_colors, lights.color, sizeof(lights.color));

                if (glow_shell)
                {
                    // Nothing in the pass reads r_blend, and the alpha the engine leaves standing
                    // behind the flat color is 1; only the alpha test of a masked mesh still sees
                    // it, which is why it is set rather than left as the entity's.
                    pending_.blend = 1.0f;

                    pending_.shell_color = ShellColor(entity);
                    pending_.chrome_origin = ShellChromeOrigin(view);
                }
                else
                {
                    // The engine builds chrome around the view: SetChromeOrigin stores the view
                    // origin, and vright is the view's right vector. Both are derivable from the
                    // frame's refdef, so no extra engine global has to be reached for.
                    pending_.shell_color = ncl_math::Vector3{};
                    pending_.chrome_origin = ncl_math::Vector3::Ref(preview != nullptr ? preview->camera.origin : view.origin);
                }

                float forward[3];
                float up[3];
                if (preview != nullptr)
                {
                    std::copy(std::begin(preview->view_right), std::end(preview->view_right), pending_.view_right.data());
                }
                else
                {
                    ncl_math::AngleVectors(view.angles, forward, pending_.view_right.data(), up);
                }
            }
        }

        pending_.seen_parts++;

        if (recording)
        {
            submission_.entity_lights = pending_.light_count;
        }

        size_t queued_before = pending_.mesh_count;

        for (uint32_t i = 0; i < range->mesh_count; i++)
        {
            const ncl_studio::MeshRange& mesh = gpu->mesh->meshes[range->first_mesh + i];

            if (mesh.index_count == 0)
            {
                continue;
            }

            const goldsrc::mstudiotexture_t* texture = ResolveTexture(texture_header, skin_family, mesh.skinref);

            if (texture == nullptr || texture->width <= 0 || texture->height <= 0)
            {
                // Back to the engine rather than skipped. The engine indexes the skin table
                // unconditionally and draws the mesh; dropping it here is the one path that
                // makes geometry disappear instead of being drawn by somebody.
                return RejectSubmission("mesh skin reference does not resolve to a texture", queued_before, true);
            }

            const goldsrc::mstudiotexture_t* textures_base = reinterpret_cast<const goldsrc::mstudiotexture_t*>(
                reinterpret_cast<const uint8_t*>(texture_header) + texture_header->textureindex
            );

            // A null material is the legacy path exactly: no program of its own, no uniforms
            // uploaded. Suppressing every source at once is what lets an A/B against the engine
            // mean something on an installation that has material content, and no guard on one
            // source could do that, because a model document sits next to the model itself.
            ncl_material::MaterialOrigin material_origin = ncl_material::MaterialOrigin::Implicit;
            const bool check_rigid_material = pending_.rigid_bindings != nullptr;
            const ncl_material::Material* material = materials_enabled ? models_.ResolveMaterial(
                                                                             model,
                                                                             header,
                                                                             texture_header,
                                                                             static_cast<int>(texture - textures_base),
                                                                             model_draw_.texture_model,
                                                                             get_resource_barrier(),
                                                                             check_rigid_material ? &material_origin : nullptr
                                                                         )
                                                                       : nullptr;
            bool authored_material = material != nullptr && material_origin != ncl_material::MaterialOrigin::Implicit;

            if (serial != inputs.live_serial)
            {
                return Reject("studio model was invalidated during material preparation");
            }

            bool server_bound = false;
            if (materials_enabled && entity != nullptr)
            {
                EntityMaterialQuery query;
                query.entity_index = entity->index;
                query.texture_name = texture->name;

                if (const ncl_material::Material* bound = server_materials_.BindingFor(query, engine_.is_network_session()))
                {
                    material = bound;
                    authored_material = true;
                    server_bound = true;

                    if (recording)
                    {
                        submission_.server_bound++;
                    }
                }
            }

            // Above the server's binding, and only while a panel holds a session: an author
            // moving a slider judges the change on the model in front of them, and a layer that
            // lost to any other source would be a layer that sometimes does nothing. Inside the
            // materials_enabled guard, so the comparison against no material at all still shows
            // no material at all.
            if (materials_enabled && !engine_.is_network_session())
            {
                if (const ncl_material::Material* edited = edits_.FindOverride(model->name, texture->name))
                {
                    material = edited;
                    authored_material = true;
                }
            }

            const StudioMaterialBindings* bindings = nullptr;
            const ncl_gl::Program* mesh_program = material_runtime_.ProgramFor(material, &bindings);

            if (mesh_program == nullptr)
            {
                // A material naming a program that will not build must not silently render
                // as if it had none; the model goes back to the engine instead.
                return RejectSubmission("material program failed to build", queued_before, true);
            }

            int mesh_flags = texture->flags | forced_face_flags;

            unsigned int captured_texture = 0;
            bool skin_captured = false;

            for (size_t captured_index = 0; captured_index < pending_.mesh_count; ++captured_index)
            {
                const StudioQueuedMesh& captured = pending_.meshes[captured_index];
                if (captured.skin == static_cast<int>(texture - textures_base))
                {
                    captured_texture = captured.texture;
                    skin_captured = true;
                    break;
                }
            }

            if (!skin_captured)
            {
                // Resolve remap and forced chrome while the engine still owns this entity/pass.
                captured_texture = engine_.CaptureSkin(*api, texture_header, static_cast<int>(texture - textures_base));
            }

            {
                performance::LocalCpuScope preparation(monitor_.get_studio_detail_monitor(), performance::PerfStage::StudioDataPrepare);
                if (pending_.mesh_count == pending_.meshes.size())
                {
                    pending_.meshes.emplace_back();
                }

                StudioQueuedMesh& item = pending_.meshes[pending_.mesh_count];
                item.images = StudioMeshImages{};
                item.program = mesh_program;
                item.material_bindings = bindings;
                if (check_rigid_material && (authored_material || mesh_program != program))
                {
                    pending_.rigid_bindings = nullptr;
                }

                if (material != nullptr && material->program != ncl_material::kLegacyProgram)
                {
                    item.material = *material;
                    ServerMaterials_ApplyEffectPolicy(*item.material, server_bound);
                }
                else
                {
                    item.material.reset();
                }

                item.skin = static_cast<int>(texture - textures_base);
                item.texture = captured_texture;
                item.texel_scale[0] = 1.0f / texture->width;
                item.texel_scale[1] = 1.0f / texture->height;
                item.masked = (mesh_flags & goldsrc::STUDIO_NF_MASKED) != 0;
                item.chrome = (mesh_flags & goldsrc::STUDIO_NF_CHROME) != 0;
                item.shade_mode = ShadeModeFor(mesh_flags, glow_shell, rendermode);

                // Texture additive flags override blending only in kRenderNormal. Other entity
                // modes retain the blend state captured from GL_SetRenderMode.
                item.additive = (mesh_flags & goldsrc::STUDIO_NF_ADDITIVE) != 0 && rendermode == goldsrc::kRenderNormal;
                item.index_count = static_cast<GLsizei>(mesh.index_count);
                item.index_offset = static_cast<uintptr_t>(mesh.first_index) * sizeof(uint32_t);
                ++pending_.mesh_count;

                if (recording)
                {
                    submission_.meshes++;
                    submission_.shade_modes |= 1u << item.shade_mode;

                    if (material != nullptr)
                    {
                        submission_.materials++;
                    }
                }
            }
        }

        // A completed pass can leave the engine scope because every mutable draw input is owned.
        if (pending_.seen_parts >= pending_.expected_parts)
        {
            FlushPending();
        }

        return true;
    }

    bool StudioRenderer::TryDraw(const StudioModelInputs& inputs)
    {
        goldsrc::model_t* model = inputs.model;
        const goldsrc::studiohdr_t* header = inputs.header;
        void* submodel = inputs.submodel;

        const uint64_t epoch = cancellation_epoch_;
        bool drawn = StudioRenderer::TryDrawSubmission(inputs);
        if (drawn && (inputs.serial != inputs.live_serial || epoch != cancellation_epoch_))
        {
            drawn = Reject("studio model was invalidated during image preparation");
        }

        if (!drawn)
        {
            RejectPreview(inputs.entity);
            abandoned_ = true;
            abandoned_serial_ = inputs.live_serial;
        }

        if (diagnostics_.is_armed())
        {
            StudioOutcome outcome;
            outcome.model = inputs.serial == inputs.live_serial && model != nullptr ? model->name : "";
            outcome.header = inputs.serial == inputs.live_serial && header != nullptr ? header->name : "";
            outcome.reason = drawn ? "" : StudioRenderer::get_last_rejection();
            outcome.drawn = drawn;

            diagnostics_.Note(submission_, outcome);
        }

        return drawn;
    }

    const char* StudioRenderer::get_last_rejection() const
    {
        return last_rejection_;
    }

    void StudioRenderer::WarmPrograms()
    {
        material_runtime_.WarmPrograms();
    }

    void StudioRenderer::OnGammaChanged(const std::array<float, kStudioLightGammaEntries>& light_gamma)
    {
        material_runtime_.SetLightGamma(light_gamma);
        light_gamma_ready_ = true;
    }

    StudioRenderer::StudioRenderer(
        RenderDevice& device,
        ShaderManager& shaders,
        MaterialManifestCatalog& manifests,
        MaterialTextureCache& textures,
        StudioModelCache& models,
        ServerMaterials& server_materials,
        const MaterialEditSession& edits,
        StudioEffects& effects,
        StudioEngineBindings& engine,
        StudioDiagnostics& diagnostics,
        MaterialCoverageRuntime& coverage,
        performance::PerformanceMonitor& monitor,
        performance::PerformanceMapper& mapper
    ) :
        monitor_(monitor),
        device_(device),
        shaders_(shaders),
        textures_(textures),
        coverage_(coverage),
        models_(models),
        server_materials_(server_materials),
        edits_(edits),
        effects_(effects),
        engine_(engine),
        diagnostics_(diagnostics),
        material_runtime_(shaders, manifests, textures, monitor),
        submission_sink_(
            device,
            material_runtime_,
            {BindCapturedTexture, NoteProgramBind, this},
            monitor,
            mapper,
            studio_depth_,
            &diagnostics.get_phase_capture()
        ),
        queue_(submission_sink_, {this, NoteQueueFlush, NoteImmediate})
    {
        device_.Register(this);
    }

    StudioRenderer::~StudioRenderer()
    {
        device_.Unregister(this);
    }

    void StudioRenderer::OnModelBoundary()
    {
        ++preparation_epoch_;
    }

    void StudioRenderer::ClearDiagnostics()
    {
        diagnostics_.Clear();
        diagnostics_.get_phase_capture().SessionBoundary();
    }

    void StudioRenderer::OnDeviceLost()
    {
        DiscardPending();
        InvalidateContextReferences();
    }

    void StudioRenderer::BindCapturedTexture(void* user, unsigned int texture)
    {
        static_cast<StudioRenderer*>(user)->engine_.BindCapturedTexture(texture);
    }

    bool StudioRenderer::MaterialHasEffects(void* user, const ncl_material::Material& material)
    {
        return static_cast<const StudioEffects*>(user)->CheckEnabled(material);
    }

    StudioModelBarrier StudioRenderer::get_resource_barrier()
    {
        return {this, [](void* user) { static_cast<StudioRenderer*>(user)->Flush(StudioFlushReason::Resource); }};
    }

    const StudioPreviewInputs* StudioRenderer::PreviewInputs(const goldsrc::cl_entity_t* entity) const
    {
        return preview_inputs_ != nullptr && preview_inputs_->entity == entity ? preview_inputs_ : nullptr;
    }

    void StudioRenderer::CapturePreviewPacket(const goldsrc::cl_entity_t* entity, const StudioDrawPacket& packet)
    {
        const StudioPreviewInputs* preview = PreviewInputs(entity);
        if (preview != nullptr && preview->capture != nullptr)
        {
            preview->capture->Capture(packet);
        }
    }

    void StudioRenderer::RejectPreview(const goldsrc::cl_entity_t* entity)
    {
        if (preview_inputs_ != nullptr && preview_inputs_->entity == entity)
        {
            preview_inputs_->rejected = true;
        }
    }
} // namespace render
