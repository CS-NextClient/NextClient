#include "engine.h"

#include <algorithm>
#include <cstring>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <ncl_material/ProgramManifest.h>
#include <ncl_math/vec3.h>
#include <goldsrc/studio_api.h>
#include <goldsrc/format/studio.h>
#include "graphics/render/GpuBuffer.h"
#include "graphics/render/RenderDevice.h"

#include "common/cvar.h"
#include "graphics/gl_local.h"
#include "graphics/view.h"
#include "graphics/render/MaterialEdit.h"
#include "graphics/render/MaterialManifests.h"
#include "graphics/render/MaterialTextures.h"
#include "graphics/render/RenderConfig.h"
#include "graphics/render/ShaderManager.h"
#include "graphics/render/StudioDraw.h"
#include "graphics/render/StudioDump.h"
#include "graphics/render/ServerMaterials.h"
#include "graphics/render/StudioGeometry.h"
#include "graphics/render/StudioIntercept.h"

namespace
{
    enum
    {
        kUniformTexelScale = 0,
        kUniformAmbient,
        kUniformShade,
        kUniformColorMix,
        kUniformLightVec,
        kUniformShadeMode,
        kUniformChrome,
        kUniformChromeOrigin,
        kUniformViewRight,
        kUniformBlend,
        kUniformGlowScale,
        kUniformShellColor,
        kUniformEntityLightCount,
        kUniformEntityLight,
        kUniformEntityLightColor,
        kUniformGamma,
        kUniformFlipped,
        kUniformBoneScale,
        kUniformCount
    };

    constexpr const char* kUniformNames[kUniformCount] = {
        "uTexelScale",
        "uAmbient",
        "uShade",
        "uColorMix",
        "uLightVec",
        "uShadeMode",
        "uChrome",
        "uChromeOrigin",
        "uViewRight",
        "uBlend",
        "uGlowScale",
        "uShellColor",
        "uEntityLightCount",
        "uEntityLight",
        "uEntityLightColor",
        "uGamma",
        "uFlipped",
        "uBoneScale"
    };

    static_assert(kUniformCount <= ncl_gl::kMaxUniforms, "the program's location array is fixed and is written without a bound");

    constexpr int kLightGammaEntries = 256;

    // Both gamma tables hold 1024 entries and are addressed by an 8-bit value.
    constexpr int kGammaTableStride = 4;

// What ShadeModeFor returns, and what uShadeMode is compared against in the shaders.
// Macros so that the same values feed both the C++ constants and the define prologue
// injected into every studio-family program; the shaders carry no copy of their own.
#define NCL_SHADE_LIT 0
#define NCL_SHADE_FLAT 1
#define NCL_SHADE_FULLBRIGHT 2
#define NCL_SHADE_ADDITIVE 3
#define NCL_SHADE_GLOW_SHELL 4
#define NCL_SHADE_STR2(value) #value
#define NCL_SHADE_STR(value) NCL_SHADE_STR2(value)

    constexpr int kShadeLit = NCL_SHADE_LIT;
    constexpr int kShadeFlat = NCL_SHADE_FLAT;
    constexpr int kShadeFullbright = NCL_SHADE_FULLBRIGHT;
    constexpr int kShadeAdditive = NCL_SHADE_ADDITIVE;
    constexpr int kShadeGlowShell = NCL_SHADE_GLOW_SHELL;

    constexpr const char* kShadeDefines =
        "#define kShadeLit " NCL_SHADE_STR(NCL_SHADE_LIT) "\n"
        "#define kShadeFlat " NCL_SHADE_STR(NCL_SHADE_FLAT) "\n"
        "#define kShadeFullbright " NCL_SHADE_STR(NCL_SHADE_FULLBRIGHT) "\n"
        "#define kShadeAdditive " NCL_SHADE_STR(NCL_SHADE_ADDITIVE) "\n"
        "#define kShadeGlowShell " NCL_SHADE_STR(NCL_SHADE_GLOW_SHELL) "\n";

    constexpr render::ProgramDesc kProgram = {
        "studio.vert",
        "studio.frag",
        kShadeDefines,
        kUniformNames,
        kUniformCount,
        nullptr,
        0
    };

    constexpr GLuint kBonesBindingPoint = 0;
    constexpr size_t kBoneMatrixFloats = 3 * 4;
    constexpr size_t kBonesBufferSize = goldsrc::MAXSTUDIOBONES * kBoneMatrixFloats * sizeof(float);

    render::GpuBuffer g_Bones;

    // Room for one scaled copy of the transforms. Static because it is filled and uploaded
    // inside one call, and a stack copy of it is eight kilobytes.
    float g_ScaledBones[goldsrc::MAXSTUDIOBONES * kBoneMatrixFloats];

    // The engine's studio renderer has no notion of an entity's scale - it uses that field
    // for sprites and nothing else - so honouring it for everything the game draws would be
    // this path disagreeing with the engine over a field a server may be using for something
    // else entirely, and an A/B against the engine would never come out clean. It exists for
    // the editor's stand-in, so it applies while the editor holds a panel and not otherwise.
    float EntityScale(const goldsrc::cl_entity_t* entity)
    {
        if (entity == nullptr || !render::MaterialEdit_PanelOpen())
        {
            return 1.0f;
        }

        // Zero is what every entity nobody has scaled carries, and it means "as built".
        return entity->curstate.scale > 0.0f ? entity->curstate.scale : 1.0f;
    }

    // Scales the transforms the engine has already computed: each bone is pushed away from
    // the entity's own origin by the factor and its rotation grows with it, which is a
    // uniform scale about the model's own root.
    //
    // The rows stop being orthonormal, and the entity light term reads them as an inverse
    // rotation, which is why the factor is handed to the shader as well.
    const float* ScaleBones(const float* bones, int count, const float* origin, float scale)
    {
        std::memcpy(g_ScaledBones, bones, sizeof(g_ScaledBones));

        int limit = std::min(count, static_cast<int>(goldsrc::MAXSTUDIOBONES));

        for (int bone = 0; bone < limit; bone++)
        {
            const float* source = bones + bone * kBoneMatrixFloats;
            float* target = g_ScaledBones + bone * kBoneMatrixFloats;

            for (int row = 0; row < 3; row++)
            {
                target[row * 4 + 0] = source[row * 4 + 0] * scale;
                target[row * 4 + 1] = source[row * 4 + 1] * scale;
                target[row * 4 + 2] = source[row * 4 + 2] * scale;
                target[row * 4 + 3] = origin[row] + (source[row * 4 + 3] - origin[row]) * scale;
            }
        }

        return g_ScaledBones;
    }
    GLuint g_ConfiguredProgram;
    uint32_t g_ConfiguredGeneration;
    bool g_LightGammaDirty = true;

    // Per model draw, not per submodel: the bone transforms and the texture header are
    // the same for every body part of one model, and resolving or uploading them again
    // for each one was measurable work for nothing.
    struct ModelDrawCache
    {
        uint32_t serial;
        const goldsrc::studiohdr_t* header;
        const goldsrc::cl_entity_t* entity;
        const goldsrc::studiohdr_t* texture_header;
        bool bones_uploaded;
    };

    ModelDrawCache g_ModelDraw;

    // Set when a submission is rejected after the queue was armed. The whole model has to
    // follow it back to the engine: the flush that empties the queue also disarms it, so a
    // later body part would re-arm with its own part counter, never reach the model's count,
    // and leave its meshes to be drawn outside the state the engine set up for this model.
    uint32_t g_AbandonedSerial;
    bool g_Abandoned;

    const ncl_gl::Program* g_Program;
    uint32_t g_ProgramGeneration;

    // Shaders_Get builds a string key and searches the cache, which is a heap allocation
    // on a path that runs once per submodel. The generation tells us in one comparison
    // whether the cached pointer is still the right one.
    const ncl_gl::Program* CurrentProgram()
    {
        uint32_t generation = render::Shaders_Generation();

        if (g_ProgramGeneration != generation)
        {
            g_ProgramGeneration = generation;
            g_Program = render::Shaders_Get(kProgram);
        }

        return g_Program;
    }
    const char* g_LastRejection = "not attempted";
    char g_RejectionDetail[256];

    bool Reject(const char* reason)
    {
        g_LastRejection = reason;
        return false;
    }


    // A mismatch here means either a shifted offset or a pointer from a different
    // allocation altogether, and only the raw numbers tell those two apart.
    bool RejectSubModel(const goldsrc::studiohdr_t* header, void* submodel, const ncl_studio::StudioMesh& mesh)
    {
        ptrdiff_t direct =
            static_cast<const uint8_t*>(submodel) - reinterpret_cast<const uint8_t*>(header);
        ptrdiff_t indirect =
            *static_cast<const uint8_t* const*>(submodel) - reinterpret_cast<const uint8_t*>(header);

        int written = V_snprintf(
            g_RejectionDetail,
            sizeof(g_RejectionDetail),
            "submodel offsets from header %p: direct %d, indirect %d; converted:",
            static_cast<const void*>(header),
            static_cast<int>(direct),
            static_cast<int>(indirect)
        );

        for (size_t i = 0; i < mesh.submodels.size() && written > 0 && written < static_cast<int>(sizeof(g_RejectionDetail)); i++)
        {
            written += V_snprintf(
                g_RejectionDetail + written,
                sizeof(g_RejectionDetail) - written,
                " %u",
                mesh.submodels[i].file_offset
            );
        }

        g_LastRejection = g_RejectionDetail;

        return false;
    }

    bool EnsureBones()
    {
        if (g_Bones.ok())
        {
            return true;
        }

        return g_Bones.Create(GL_UNIFORM_BUFFER, nullptr, kBonesBufferSize, GL_DYNAMIC_DRAW);
    }

    void ConfigureProgram(const ncl_gl::Program& program)
    {
        // The generation is part of the key because a GL program name is not an identity:
        // a reload deletes the object and the driver hands the same name to the relinked
        // one, which has none of these uniforms yet.
        uint32_t generation = render::Shaders_Generation();

        if (g_ConfiguredProgram == program.prog && g_ConfiguredGeneration == generation && !g_LightGammaDirty)
        {
            return;
        }

        g_ConfiguredProgram = program.prog;
        g_ConfiguredGeneration = generation;
        g_LightGammaDirty = false;

        GLuint block = glGetUniformBlockIndex(program.prog, "Bones");
        if (block != GL_INVALID_INDEX)
        {
            glUniformBlockBinding(program.prog, block, kBonesBindingPoint);
        }

        // The engine indexes lightgammatable at illum * 4 for integer illum in 0..255, so
        // only those 256 entries can ever be read and the array is exact, not a resample.
        float light_gamma[kLightGammaEntries];
        for (int i = 0; i < kLightGammaEntries; i++)
        {
            light_gamma[i] = static_cast<float>(lightgammatable[i * kGammaTableStride]) / 1023.0f;
        }

        GLint previous_program = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);

        glUseProgram(program.prog);
        glUniform1i(glGetUniformLocation(program.prog, "uTexture"), 0);
        glUniform1fv(glGetUniformLocation(program.prog, "uLightGamma"), kLightGammaEntries, light_gamma);
        glUseProgram(static_cast<GLuint>(previous_program));
    }

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

    // StudioSetupModel writes the address of the engine's own submodel pointer into its
    // out parameter rather than the pointer itself. Rather than commit to one reading of
    // a stock engine we cannot inspect, both are tried and the one that resolves to a
    // converted range wins. The rejected candidate is only ever compared as an offset,
    // never dereferenced, so a wrong guess cannot fault.
    const ncl_studio::SubModelRange* ResolveSubModel(
        void* raw,
        const goldsrc::studiohdr_t* header,
        const ncl_studio::StudioMesh& mesh
    )
    {
        if (raw == nullptr)
        {
            return nullptr;
        }

        const uint8_t* base = reinterpret_cast<const uint8_t*>(header);

        const uint8_t* direct = static_cast<const uint8_t*>(raw);
        const uint8_t* indirect = *static_cast<const uint8_t* const*>(raw);

        for (const uint8_t* candidate : {direct, indirect})
        {
            if (candidate == nullptr || candidate < base)
            {
                continue;
            }

            const ncl_studio::SubModelRange* range =
                FindByOffset(mesh, static_cast<uint32_t>(candidate - base));

            if (range != nullptr)
            {
                return range;
            }
        }

        return nullptr;
    }

    // Textures may live in the model itself or in a companion <name>T.mdl, and the skin
    // table lives in whichever of the two holds them, not in the model header.
    const goldsrc::studiohdr_t* ResolveTextureHeader(
        const goldsrc::engine_studio_api_t* api,
        goldsrc::model_t* model,
        const goldsrc::studiohdr_t* header
    )
    {
        if (header->textureindex != 0)
        {
            return header;
        }

        if (api->Mod_ForName == nullptr || api->Mod_Extradata == nullptr)
        {
            return nullptr;
        }

        char name[64 + 2];
        size_t length = V_strlen(model->name);

        if (length < 4 || length + 2 > sizeof(name))
        {
            return nullptr;
        }

        V_strcpy_safe(name, model->name);
        V_strcpy(name + length - 4, "T.mdl");

        goldsrc::model_t* texture_model = api->Mod_ForName(name, 0);
        if (texture_model == nullptr)
        {
            return nullptr;
        }

        return static_cast<const goldsrc::studiohdr_t*>(api->Mod_Extradata(texture_model));
    }

    int ActiveSkinFamily(const goldsrc::cl_entity_t* entity, const goldsrc::studiohdr_t* texture_header, const char* model_name)
    {
        // The panel's own choice outranks the entity, and only for the model it is editing.
        // Which family is in force decides which texture a mesh resolves to, and therefore
        // which material it gets, so an author has to be able to see the other ones.
        int preview = model_name != nullptr ? render::MaterialEdit_ShownSkin(model_name) : -1;

        int skin = preview >= 0 ? preview : (entity != nullptr ? entity->curstate.skin : 0);

        return (skin > 0 && skin < texture_header->numskinfamilies) ? skin : 0;
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

    goldsrc::cvar_t* g_RightHand;
    bool g_RightHandResolved;

    // GoldSrc mirrors the view model for left handed players, which reverses the winding
    // of every triangle. The engine answers that by not culling at all rather than by
    // flipping the cull face.
    bool IsFlippedViewModel(const goldsrc::engine_studio_api_t* api)
    {
        if (!g_RightHandResolved)
        {
            g_RightHandResolved = true;
            g_RightHand = Cvar_FindVar("cl_righthand");
        }

        if (g_RightHand == nullptr || g_RightHand->value <= 0.0f || api->GetCurrentEntity == nullptr)
        {
            return false;
        }

        goldsrc::cl_entity_t* entity = api->GetCurrentEntity();

        return entity != nullptr && entity->index == cl->viewent.index;
    }

    // kShadeFullbright is a deliberate addition: the stock engine only declares
    // STUDIO_NF_FULLBRIGHT and never reads it, so a model authored with it is currently lit
    // like any other. Honouring it makes the shader path differ from the original on
    // purpose, and the A/B comparison will differ for such models.
    int ShadeModeFor(int flags, bool glow_shell, int rendermode)
    {
        // The shell is neither lit nor blended: the engine's forced chrome branch emits no
        // vertex colour at all, so every vertex of it keeps the flat colour the pass was
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
        // it: no skin bound, a flat colour, a sphere map driven from an orbiting point. A
        // different forced flag would keep the engine's ordinary lighting under the
        // additive render mode the forcing selects, which is not reproduced here.
        if ((forced_face_flags & ~goldsrc::STUDIO_NF_CHROME) != 0)
        {
            return "forced face flags other than chrome are set";
        }

        // What the shell is drawn with beyond the flag itself - its colour, how far the
        // vertices move out - is read off the entity, so a pass without one is a pass this
        // cannot set up.
        if (forced_face_flags != 0 && entity == nullptr)
        {
            return "forced face flags with no current entity";
        }

        return nullptr;
    }

    // What the engine draws with, which is not always what the entity asked for: forced
    // face flags turn the whole model additive, glow shell being the case that sets them.
    //
    // kRenderTransColor asks for nothing of its own, which took disassembling the shipped
    // GL_SetRenderMode to establish rather than believe: for that mode it sets the ordinary
    // alpha blend and then asks for a texture environment mode of GL_ALPHA, which is not
    // one, so the driver refuses the call and the environment stays as it was. Unlike every
    // other mode it sets neither the colour nor the depth mask, and the draw loop writes a
    // colour per vertex anyway. What is left is the translucent path.
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

    struct EntityLights
    {
        int count;
        float light[kEntityLights][4];   // xyz origin, w radius squared
        float color[kEntityLights][3];   // through the linear table, so 0..1023
    };

    // R_StudioEntityLight. A light keyed to this entity has already been moved onto it by
    // the engine, which the client asks for before the model is drawn, so the origins are
    // taken as they stand. The engine's own replacement of the weakest of three reads as a
    // decompiler artefact in the reconstruction; with three or fewer lights, which is what
    // a scene has, the two agree either way.
    void SelectEntityLights(const goldsrc::cl_entity_t* entity, EntityLights& out)
    {
        out.count = 0;

        if (entity == nullptr || p_cl_elights == nullptr)
        {
            return;
        }

        float strengths[kEntityLights] = {};

        for (int i = 0; i < kMaxElights; i++)
        {
            const goldsrc::dlight_t& light = (*p_cl_elights)[i];

            if (!Elight_Alive(light))
            {
                continue;
            }

            float distance2 = ncl_math::DistanceSqr(
                ncl_math::Vector3::Ref(entity->origin),
                ncl_math::Vector3::Ref(light.origin)
            );

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

            // The engine takes the colour through the linear table before it adds anything,
            // and adds in that domain; the table is ours, so this half is exact.
            out.color[slot][0] = static_cast<float>(lineargammatable[kGammaTableStride * light.color.r]);
            out.color[slot][1] = static_cast<float>(lineargammatable[kGammaTableStride * light.color.g]);
            out.color[slot][2] = static_cast<float>(lineargammatable[kGammaTableStride * light.color.b]);
        }
    }

    // The exponent the linear and screen tables are built from, rather than the tables:
    // both are pure powers of it, and 2048 uniform floats would not fit the guaranteed
    // vertex uniform budget. What is lost is the tables' rounding to integers.
    //
    // Read from the pointer the gamma build itself uses, which is re-resolved on every
    // rebuild: a pointer of our own would have to be kept correct against the same events
    // and could disagree with the tables it is meant to reproduce.
    ncl_math::Vector2 GammaExponents()
    {
        float gamma = gamma_cvar != nullptr && gamma_cvar->value > 0.0f ? gamma_cvar->value : 2.5f;

        return ncl_math::Vector2 {gamma, 1.0f / gamma};
    }

    ncl_math::Vector3 ShellColor(const goldsrc::cl_entity_t* entity)
    {
        return ncl_math::Vector3 {
            static_cast<float>(entity->curstate.rendercolor.r) / 255.0f,
            static_cast<float>(entity->curstate.rendercolor.g) / 255.0f,
            static_cast<float>(entity->curstate.rendercolor.b) / 255.0f
        };
    }

    constexpr float kShellChromeRadius = 4000.0f;
    constexpr float kShellChromeZRate = 0.33f;

    goldsrc::cvar_t* g_GlowShellFreq;
    bool g_GlowShellFreqResolved;

    // For the shell the engine replaces the chrome origin with a point orbiting the world,
    // which is what makes the sphere map crawl over a model that is standing still; the
    // ordinary one is the view origin and moves only with the camera.
    ncl_math::Vector3 ShellChromeOrigin()
    {
        if (!g_GlowShellFreqResolved)
        {
            g_GlowShellFreqResolved = true;
            g_GlowShellFreq = Cvar_FindVar("r_glowshellfreq");
        }

        double phase = (g_GlowShellFreq != nullptr ? g_GlowShellFreq->value : 0.0f) * cl->time;

        return ncl_math::Vector3 {
            static_cast<float>(std::cos(phase)) * kShellChromeRadius,
            static_cast<float>(std::sin(phase)) * kShellChromeRadius,
            static_cast<float>(std::cos(phase * kShellChromeZRate)) * kShellChromeRadius
        };
    }

    // Body parts arrive one submission call at a time, but they share bones, lighting and
    // cull state, and across them the same texture is often used again: on the custom
    // player models people actually run, grouping by texture removes about 40% of the
    // draw calls. So the calls are queued for the length of one model and issued grouped,
    // with glMultiDrawElements collapsing each group into a single call.
    constexpr int kMaxQueued = 256;

    struct QueuedMesh
    {
        const ncl_gl::Program* program;
        const ncl_material::Material* material;
        int skin;               // index into the texture header, which is what the engine binds by
        float texel_scale[2];
        bool masked;
        bool chrome;
        bool additive;
        int shade_mode;
        GLsizei index_count;
        uintptr_t index_offset;
    };

    struct PendingModel
    {
        bool active;
        ncl_math::Vector3 chrome_origin;
        ncl_math::Vector3 view_right;
        GLuint vao;
        const ncl_gl::Program* program;   // the model's default, used when a material names none
        render::StudioLighting lighting;
        bool flipped;
        float blend;
        float glow_scale;

        // What ScaleBones grew the bone rows by, so the entity light term can read them as
        // the inverse rotation they stopped being.
        float bone_scale;
        ncl_math::Vector3 shell_color;
        EntityLights lights;
        const goldsrc::studiohdr_t* texture_header;
        int rendermode;
        int expected_parts;
        int seen_parts;
        int count;
        QueuedMesh items[kMaxQueued];
    };

    PendingModel g_Pending;

    // Filled only while a dump is armed, behind one flag read per submission: the record
    // serves one frame of the process's life, and r_studio_dump is the only reader.
    render::StudioSubmission g_Submission;

    struct MaterialProgram
    {
        std::string name;
        std::string vertex;
        std::string fragment;
        render::ProgramDesc desc;

        // Resolved once per generation for the same reason CurrentProgram does it: asking
        // the manager builds a key out of the whole desc, which is a heap allocation on a
        // path that runs once per submodel.
        const ncl_gl::Program* program;
        uint32_t generation;
    };

    std::vector<std::unique_ptr<MaterialProgram>> g_MaterialPrograms;

    // A material's program is compiled from <program>.vert and <program>.frag. The entry
    // owns its name strings, so the reference stays valid once it is stored.
    MaterialProgram& MaterialEntry(const std::string& program)
    {
        for (const std::unique_ptr<MaterialProgram>& entry : g_MaterialPrograms)
        {
            if (entry->name == program)
            {
                return *entry;
            }
        }

        std::unique_ptr<MaterialProgram> entry = std::make_unique<MaterialProgram>();
        entry->name = program;
        entry->vertex = program + ".vert";
        entry->fragment = program + ".frag";
        entry->desc = render::ProgramDesc {
            entry->vertex.c_str(),
            entry->fragment.c_str(),
            kShadeDefines,
            kUniformNames,
            kUniformCount,
            nullptr,
            0
        };

        g_MaterialPrograms.push_back(std::move(entry));

        return *g_MaterialPrograms.back();
    }

    // A material names its program; the built-in one keeps the fast path free of the
    // entry lookup, and anything else goes through the material entries.
    // A custom program still receives every built-in uniform, so it only has to add what
    // it needs rather than reimplement skinning and lighting.
    const ncl_gl::Program* ProgramFor(const ncl_material::Material* material)
    {
        if (material == nullptr || material->program == ncl_material::kLegacyProgram)
        {
            return CurrentProgram();
        }

        MaterialProgram& entry = MaterialEntry(material->program);
        uint32_t generation = render::Shaders_Generation();

        if (entry.generation != generation)
        {
            entry.generation = generation;
            entry.program = render::Shaders_Get(entry.desc);
        }

        return entry.program;
    }

    // Only for a material's own parameters, and only on a custom program: the built-in
    // program declares none of them, so looking them up there would cost a GL call per
    // draw for nothing.
    // Unit 0 stays the model's own skin, so a material's artwork starts at unit 1.
    // Above every unit the engine keeps its own bookkeeping for: unit 0 is the skin it
    // tracks through GL_Bind, unit 1 the lightmap it caches in cnttextures, unit 2 the
    // detail texture. Binding into those with plain GL calls would leave the engine's cache
    // describing a unit that no longer holds what it thinks, and its next world batch would
    // skip a bind it needed. GL 3.3 guarantees sixteen fragment samplers, so staying clear
    // costs nothing.
    const std::vector<ncl_material::ParamSchema> kNoSchema;

    constexpr int kFirstMaterialUnit = 3;
    constexpr int kMaxMaterialUnit = kFirstMaterialUnit + static_cast<int>(ncl_material::kMaxSchemaTextures);

    // False when the program has no such sampler, so the unit is not consumed: the count
    // this returns to the caller decides how many units are unbound after the draw, and
    // claiming one that was never bound would clear a unit this draw never touched.
    bool BindMaterialTexture(const ncl_gl::Program& program, const std::string& name, GLuint texture, int unit)
    {
        GLint location = glGetUniformLocation(program.prog, name.c_str());
        if (location < 0)
        {
            return false;
        }

        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, texture != 0 ? texture : render::MaterialTexture_Neutral());
        glUniform1i(location, unit);

        return true;
    }

    int UploadMaterialParams(const ncl_gl::Program& program, const ncl_material::Material& material)
    {
        // Resolved before any unit is selected. A cache miss uploads the image through the
        // engine's bind, which lands on whatever unit is active, so doing this while a unit
        // is selected would overwrite the binding just made for it. The caller binds the
        // model skin after this returns, which repairs unit 0.
        for (const ncl_material::Param& param : material.params)
        {
            if (param.value.type == ncl_material::ParamType::TextureRef)
            {
                render::MaterialTexture_Get(param.value.texture_ref.c_str());
            }
        }

        for (const ncl_material::Param& param : material.params)
        {
            if (param.value.type == ncl_material::ParamType::TextureRef)
            {
                continue;
            }

            GLint location = glGetUniformLocation(program.prog, param.name.c_str());
            if (location < 0)
            {
                continue;
            }

            if (param.value.type == ncl_material::ParamType::Scalar)
            {
                glUniform1f(location, param.value.scalar);
            }
            else
            {
                glUniform4fv(location, 1, param.value.vec4.data());
            }
        }

        // Units come from the program's declaration, not from where a parameter happens to
        // sit in this material. Sampler uniforms belong to the program object and outlive
        // the draw, so a material declaring fewer textures than the previous one would
        // otherwise leave that one's samplers aimed at images this material never chose.
        // Every declared sampler is bound on every draw for the same reason.
        const ncl_material::ProgramManifest* manifest = render::Manifest_Get(material.program);
        int unit = kFirstMaterialUnit;

        // Both material sources refuse a program that declares no manifest, precisely so
        // that the samplers can be enumerated here rather than handed out by position.
        for (const ncl_material::ParamSchema& schema : manifest != nullptr ? manifest->parameters : kNoSchema)
        {
            if (schema.type != ncl_material::ParamType::TextureRef)
            {
                continue;
            }

            // The manifest parser rejects a program declaring more textures than there are
            // units, so this cannot silently drop one.
            if (unit >= kMaxMaterialUnit)
            {
                break;
            }

            GLuint texture = 0;
            if (const ncl_material::ParamValue* value = material.FindParam(schema.name))
            {
                if (value->type == ncl_material::ParamType::TextureRef)
                {
                    texture = render::MaterialTexture_Get(value->texture_ref.c_str());
                }
            }

            if (BindMaterialTexture(program, schema.name, texture, unit))
            {
                unit++;
            }
        }

        if (unit != kFirstMaterialUnit)
        {
            // Back to unit 0 before anything else runs: the engine tracks its own active
            // unit and would otherwise bind the model skin into ours.
            glActiveTexture(GL_TEXTURE0);
        }

        return unit - kFirstMaterialUnit;
    }

    void FlushPending()
    {
        const goldsrc::engine_studio_api_t* api = render::Studio_OriginalApi();

        if (!g_Pending.active || g_Pending.count == 0 || g_Pending.program == nullptr)
        {
            g_Pending.active = false;
            g_Pending.count = 0;
            return;
        }

        int order[kMaxQueued];
        for (int i = 0; i < g_Pending.count; i++)
        {
            order[i] = i;
        }

        // Program first, then texture: a program change is the more expensive switch,
        // and grouping by texture inside it is what collapses the draw calls.
        std::sort(order, order + g_Pending.count, [](int a, int b) {
            const QueuedMesh& left = g_Pending.items[a];
            const QueuedMesh& right = g_Pending.items[b];

            // Additive last, ahead of every other key. Those meshes write no depth, so
            // one issued before an opaque mesh that covers it shows through. The engine
            // draws in mesh order and does not have this problem because it does not
            // reorder; having chosen to reorder, the order has to be one that is safe.
            if (left.additive != right.additive)
            {
                return right.additive;
            }

            if (left.program != right.program)
            {
                return left.program < right.program;
            }

            return left.skin < right.skin;
        });

        GLint previous_program = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);

        GLint previous_vao = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vao);

        GLint previous_array_buffer = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_array_buffer);

        // GL_DEPTH_BUFFER_BIT is in the set because additive meshes turn the depth mask
        // off; without it that would leak into whatever the engine draws next.
        glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT | GL_DEPTH_BUFFER_BIT);

        // The engine sets the cull state inside its own submission rather than around it,
        // so replacing that stage means reproducing it here. GoldSrc culls front faces,
        // not back ones; inheriting whatever the previous stage left turns models inside
        // out.
        if (g_Pending.flipped)
        {
            glDisable(GL_CULL_FACE);
        }
        else
        {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
        }

        // Unit 0 is where the model skin goes and what every program samples uTexture from,
        // and the stage this replaces is not the only one that selects a unit; inheriting a
        // non-zero one would bind the skin somewhere the shader never reads.
        glActiveTexture(GL_TEXTURE0);

        glBindBufferBase(GL_UNIFORM_BUFFER, kBonesBindingPoint, g_Bones.name());
        glBindVertexArray(g_Pending.vao);

        const ncl_gl::Program* program = nullptr;

        GLsizei counts[kMaxQueued];
        const void* offsets[kMaxQueued];

        for (int start = 0; start < g_Pending.count;)
        {
            const QueuedMesh& head = g_Pending.items[order[start]];

            if (head.program != program)
            {
                program = head.program;
                ConfigureProgram(*program);

                glUseProgram(program->prog);
                glUniform1f(program->loc[kUniformAmbient], g_Pending.lighting.ambient);
                glUniform1f(program->loc[kUniformShade], g_Pending.lighting.shade);
                glUniform3fv(program->loc[kUniformColorMix], 1, g_Pending.lighting.color);
                glUniform3fv(program->loc[kUniformLightVec], 1, g_Pending.lighting.light_vec);
                glUniform3fv(program->loc[kUniformChromeOrigin], 1, g_Pending.chrome_origin.data());
                glUniform3fv(program->loc[kUniformViewRight], 1, g_Pending.view_right.data());
                glUniform1f(program->loc[kUniformBlend], g_Pending.blend);
                glUniform1f(program->loc[kUniformGlowScale], g_Pending.glow_scale);
                glUniform3fv(program->loc[kUniformShellColor], 1, g_Pending.shell_color.data());

                ncl_math::Vector2 gamma = GammaExponents();
                glUniform2fv(program->loc[kUniformGamma], 1, gamma.data());

                // The mirrored view model: the engine answers the mirror in the bone
                // transforms by negating the normal's Y inside the entity light term.
                glUniform1i(program->loc[kUniformFlipped], g_Pending.flipped ? 1 : 0);

                glUniform1f(program->loc[kUniformBoneScale], g_Pending.bone_scale);

                glUniform1i(program->loc[kUniformEntityLightCount], g_Pending.lights.count);

                if (g_Pending.lights.count != 0)
                {
                    glUniform4fv(program->loc[kUniformEntityLight], g_Pending.lights.count, &g_Pending.lights.light[0][0]);
                    glUniform3fv(program->loc[kUniformEntityLightColor], g_Pending.lights.count, &g_Pending.lights.color[0][0]);
                }
            }

            int end = start;
            int group = 0;

            // Grouped by the skin index rather than by the uploaded texture, because the
            // bind is now made by the engine from that index: two entries of one model can
            // name the same uploaded texture and still resolve differently, which is what a
            // remapped skin does. The sort orders by the same key, so a group is a run.
            while (end < g_Pending.count &&
                   g_Pending.items[order[end]].program == head.program &&
                   g_Pending.items[order[end]].skin == head.skin &&
                   g_Pending.items[order[end]].additive == head.additive)
            {
                const QueuedMesh& item = g_Pending.items[order[end]];
                counts[group] = item.index_count;
                offsets[group] = reinterpret_cast<const void*>(item.index_offset);
                group++;
                end++;
            }

            if (head.masked)
            {
                glEnable(GL_ALPHA_TEST);
                glAlphaFunc(GL_GREATER, 0.5f);

                // The engine writes depth for a masked mesh whatever the entity's render
                // mode turned the mask off for, and does not put it back afterwards. The
                // second half of that is order dependent and cannot survive regrouping,
                // but it only shows on a masked mesh of a translucent entity, which the
                // client already avoids by clearing the flag before the call.
                glDepthMask(GL_TRUE);
            }
            else
            {
                glDisable(GL_ALPHA_TEST);
            }

            // Per mesh rather than per model, exactly where the engine puts it: an additive
            // texture adds itself to what is behind it and writes no depth, whatever the
            // entity's own render mode already set up around the model.
            // Additive groups sort last, so this is entered once and never undone
            // inside the flush; glPopAttrib puts back what the engine had.
            if (head.additive)
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE);
                glDepthMask(GL_FALSE);
            }


            // Ahead of the skin bind, not after it: resolving a material image can upload
            // it, and an upload binds on the active unit, so unit 0 has to be established
            // once this has finished touching it.
            int material_textures = 0;
            if (head.material != nullptr && head.material->program != ncl_material::kLegacyProgram)
            {
                material_textures = UploadMaterialParams(*program, *head.material);
            }

            // The engine's own skin setup rather than a bind of our own, because two of the
            // cases it covers are not binds at all: a forced chrome pass binds nothing,
            // leaving the shell sprite the client put there, and a skin the model remaps by
            // player colour resolves to a texture built per entity. It reads the entity and
            // the forced flags as they stand, which is why the queue is flushed inside the
            // model's own draw rather than at the end of the scene.
            if (api != nullptr && api->StudioSetupSkin != nullptr)
            {
                api->StudioSetupSkin(const_cast<goldsrc::studiohdr_t*>(g_Pending.texture_header), head.skin);
            }

            glUniform2f(program->loc[kUniformTexelScale], head.texel_scale[0], head.texel_scale[1]);
            glUniform1i(program->loc[kUniformShadeMode], head.shade_mode);
            glUniform1i(program->loc[kUniformChrome], head.chrome ? 1 : 0);

            glMultiDrawElements(GL_TRIANGLES, counts, GL_UNSIGNED_INT, offsets, group);

            for (int i = 0; i < material_textures; i++)
            {
                glActiveTexture(GL_TEXTURE0 + kFirstMaterialUnit + i);
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            if (material_textures != 0)
            {
                glActiveTexture(GL_TEXTURE0);
            }

            start = end;
        }

        glBindVertexArray(static_cast<GLuint>(previous_vao));
        glBindBufferBase(GL_UNIFORM_BUFFER, kBonesBindingPoint, 0);
        glUseProgram(static_cast<GLuint>(previous_program));

        glPopAttrib();

        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_array_buffer));

        g_Pending.active = false;
        g_Pending.count = 0;
    }

    // A queue that outlived its model cannot be drawn faithfully any more: the skin bind
    // reads the entity and the forced flags as they stand, and by now they describe
    // someone else. No stock client leaves a queue behind, so dropping it surfaces the
    // anomaly instead of drawing it with another entity's skin.
    void DiscardPending(const char* reason)
    {
        if (g_Pending.active && g_Pending.count != 0)
        {
            g_LastRejection = reason;
        }

        g_Pending.active = false;
        g_Pending.count = 0;
    }

    // A reject taken after the queue was armed has to leave it empty. The engine is about
    // to draw this body part itself, so whatever this submission queued for it would be
    // drawn twice; and whatever earlier body parts queued has to go out now, while the GL
    // state the engine set up for this model is still current, rather than be carried into
    // the next model's scope or all the way to the end of the scene.
    bool RejectSubmission(const char* reason, int queued_before, bool counted)
    {
        if (g_Pending.active)
        {
            // Never upward: the cap flush inside the mesh loop can have emptied and
            // re-armed the queue since the mark was taken, and rolling forward to it would
            // re-issue records belonging to a queue that was already drawn.
            g_Pending.count = std::min(g_Pending.count, queued_before);

            if (counted)
            {
                g_Pending.seen_parts--;
            }

            FlushPending();
        }

        g_Abandoned = true;
        g_AbandonedSerial = g_ModelDraw.serial;

        return Reject(reason);
    }
}

void render::Studio_DiscardPending()
{
    DiscardPending("a queue outlived the scene and was discarded");
}

namespace render
{
    // The submission proper. Wrapped below rather than called directly, so the record
    // r_studio_dump takes is written at one exit instead of at each of the rejections.
    static bool TryDrawSubmission(goldsrc::model_t* model, const goldsrc::studiohdr_t* header, void* submodel);
}

bool render::TryDrawSubmission(goldsrc::model_t* model, const goldsrc::studiohdr_t* header, void* submodel)
{
    // One load decides whether the record is worth filling, which it is for one frame of
    // the process's life; the submission runs hundreds of times per frame.
    const bool recording = Dump_Armed();

    if (recording)
    {
        g_Submission = StudioSubmission{};
    }

    if (g_RenderCfg.studio_shader == nullptr || g_RenderCfg.studio_shader->value == 0.0f)
    {
        return Reject("cvar is off");
    }

    if (!render::Device_Available())
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
    const goldsrc::engine_studio_api_t* api = Studio_OriginalApi();
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

    uint32_t serial = Studio_ModelDrawSerial();
    const goldsrc::cl_entity_t* entity = api->GetCurrentEntity != nullptr ? api->GetCurrentEntity() : nullptr;

    if (recording)
    {
        g_Submission.entity_index = entity != nullptr ? entity->index : -1;
        g_Submission.view_entity = entity != nullptr && api->GetViewEntity != nullptr && entity == api->GetViewEntity();
    }

    if (g_Abandoned && g_AbandonedSerial == serial)
    {
        return Reject("an earlier body part of this model went back to the engine");
    }

    // The entity is part of the key as well: reusing bone transforms across entities
    // would skin the second one with the first one's pose.
    if (g_ModelDraw.serial != serial || g_ModelDraw.header != header || g_ModelDraw.entity != entity)
    {
        g_Abandoned = false;
        // Discarded rather than drawn: a queue still armed here belongs to a model whose
        // parts never all arrived, and the entity the skin bind would read is this one.
        DiscardPending("a queue from the previous model was discarded");

        g_ModelDraw.serial = serial;
        g_ModelDraw.header = header;
        g_ModelDraw.entity = entity;
        g_ModelDraw.texture_header = ResolveTextureHeader(api, model, header);
        g_ModelDraw.bones_uploaded = false;

        if (entity != nullptr)
        {
            render::ServerMaterials_NoteEntity(entity->index, model != nullptr ? model->name : "");

            // Which entity a model was drawn for is the only thing that ties a question
            // about a model to an answer that belongs to an entity, and the editing panel
            // asks exactly that question about the model in front of the author.
            render::Geometry_NoteDrawnBy(model->name, entity->index);
        }
    }

    const goldsrc::studiohdr_t* texture_header = g_ModelDraw.texture_header;
    if (texture_header == nullptr || texture_header->textureindex == 0)
    {
        return Reject("companion T.mdl with the textures is not loaded");
    }

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
    bool materials_enabled = !glow_shell &&
        (g_RenderCfg.studio_materials == nullptr || g_RenderCfg.studio_materials->value != 0.0f);

    int rendermode = EffectiveRenderMode(entity, forced_face_flags);

    if (recording)
    {
        g_Submission.forced_face_flags = forced_face_flags;
        g_Submission.renderfx = entity != nullptr ? entity->curstate.renderfx : 0;
        g_Submission.rendermode = entity != nullptr ? entity->curstate.rendermode : 0;
        g_Submission.effective_rendermode = rendermode;
        g_Submission.blend = p_r_blend != nullptr ? *p_r_blend : 1.0f;
        g_Submission.glow_shell = glow_shell;
        g_Submission.glow_scale = GlowShellScale(entity);

        if (glow_shell)
        {
            g_Submission.shell_color = ShellColor(entity);
        }
    }

    int skin_family = ActiveSkinFamily(entity, texture_header, model->name);

    const StudioLighting& lighting = Studio_Lighting();
    if (!lighting.valid)
    {
        return Reject("no lighting captured, StudioSetupLighting was not seen");
    }

    const StudioGpuModel* gpu = Geometry_Get(model, header);
    if (gpu == nullptr)
    {
        return Reject("geometry conversion or upload failed");
    }

    const ncl_studio::SubModelRange* range = ResolveSubModel(submodel, header, *gpu->mesh);
    if (range == nullptr)
    {
        // Nothing of this body part is queued yet, but earlier ones may be.
        RejectSubmission("", g_Pending.count, false);

        return RejectSubModel(header, submodel, *gpu->mesh);
    }

    if (recording)
    {
        g_Submission.submodel_offset = range->file_offset;
    }

    const ncl_gl::Program* program = CurrentProgram();
    if (program == nullptr)
    {
        return Reject("studio program failed to build");
    }

    if (!EnsureBones())
    {
        return Reject("bone uniform buffer could not be created");
    }

    ConfigureProgram(*program);

    if (!g_ModelDraw.bones_uploaded)
    {
        const float* bones = reinterpret_cast<const float*>(api->StudioGetBoneTransform());
        if (bones == nullptr)
        {
            return Reject("bone transforms unavailable");
        }

        float scale = EntityScale(entity);

        if (scale != 1.0f)
        {
            bones = ScaleBones(bones, header->numbones, entity->origin, scale);
        }

        if (!g_Bones.Update(bones, kBonesBufferSize))
        {
            return Reject("bone transforms unavailable");
        }

        g_ModelDraw.bones_uploaded = true;
    }

    g_LastRejection = "none";

    if (!g_Pending.active)
    {
        g_Pending.active = true;
        g_Pending.count = 0;
        g_Pending.seen_parts = 0;
        g_Pending.expected_parts = header->numbodyparts;
        g_Pending.vao = gpu->vao;
        g_Pending.program = program;
        g_Pending.lighting = lighting;
        g_Pending.flipped = IsFlippedViewModel(api);
        g_Pending.blend = p_r_blend != nullptr ? *p_r_blend : 1.0f;
        g_Pending.glow_scale = GlowShellScale(entity);
        g_Pending.bone_scale = EntityScale(entity);
        g_Pending.rendermode = rendermode;
        g_Pending.texture_header = texture_header;

        SelectEntityLights(entity, g_Pending.lights);

        if (glow_shell)
        {
            // Nothing in the pass reads r_blend, and the alpha the engine leaves standing
            // behind the flat colour is 1; only the alpha test of a masked mesh still sees
            // it, which is why it is set rather than left as the entity's.
            g_Pending.blend = 1.0f;

            g_Pending.shell_color = ShellColor(entity);
            g_Pending.chrome_origin = ShellChromeOrigin();
        }
        else
        {
            // The engine builds chrome around the view: SetChromeOrigin stores the view
            // origin, and vright is the view's right vector. Both are derivable from the
            // frame's refdef, so no extra engine global has to be reached for.
            g_Pending.chrome_origin = ncl_math::Vector3::Ref(r_refdef->vieworg);
        }

        float forward[3];
        float up[3];
        ncl_math::AngleVectors(r_refdef->viewangles, forward, g_Pending.view_right.data(), up);
    }

    g_Pending.seen_parts++;

    if (recording)
    {
        g_Submission.entity_lights = g_Pending.lights.count;
    }

    int queued_before = g_Pending.count;

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
        const ncl_material::Material* material = materials_enabled
            ? Geometry_Material(model, header, texture_header, static_cast<int>(texture - textures_base))
            : nullptr;

        if (materials_enabled && entity != nullptr)
        {
            render::EntityMaterialQuery query;
            query.entity_index = entity->index;
            query.texture_name = texture->name;

            if (const ncl_material::Material* bound = render::ServerMaterials_BindingFor(query))
            {
                material = bound;

                if (recording)
                {
                    g_Submission.server_bound++;
                }
            }
        }

        // Above the server's binding, and only while a panel holds a session: an author
        // moving a slider judges the change on the model in front of them, and a layer that
        // lost to any other source would be a layer that sometimes does nothing. Inside the
        // materials_enabled guard, so the comparison against no material at all still shows
        // no material at all.
        if (materials_enabled)
        {
            if (const ncl_material::Material* edited = render::MaterialEdit_Override(model->name, texture->name))
            {
                material = edited;
            }
        }

        const ncl_gl::Program* mesh_program = ProgramFor(material);
        if (mesh_program == nullptr)
        {
            // A material naming a program that will not build must not silently render
            // as if it had none; the model goes back to the engine instead.
            return RejectSubmission("material program failed to build", queued_before, true);
        }

        if (g_Pending.count == kMaxQueued)
        {
            // Deep enough to hit the cap is already unusual; drawing what is queued and
            // carrying on keeps it correct rather than dropping meshes.
            int expected = g_Pending.expected_parts;
            int seen = g_Pending.seen_parts;

            FlushPending();

            g_Pending.active = true;
            g_Pending.expected_parts = expected;
            g_Pending.seen_parts = seen;
            g_Pending.vao = gpu->vao;
            g_Pending.program = program;
            g_Pending.lighting = lighting;
            g_Pending.flipped = IsFlippedViewModel(api);
        }

        int mesh_flags = texture->flags | forced_face_flags;

        QueuedMesh& item = g_Pending.items[g_Pending.count++];
        item.program = mesh_program;
        item.material = material;
        item.skin = static_cast<int>(texture - textures_base);
        item.texel_scale[0] = 1.0f / texture->width;
        item.texel_scale[1] = 1.0f / texture->height;
        item.masked = (mesh_flags & goldsrc::STUDIO_NF_MASKED) != 0;
        item.chrome = (mesh_flags & goldsrc::STUDIO_NF_CHROME) != 0;
        item.shade_mode = ShadeModeFor(mesh_flags, glow_shell, rendermode);

        // Only at kRenderNormal, which is the opposite of what the reconstruction says.
        // The test level puts three entities on one additive model file, at render modes
        // normal, 4 and 5, and only that reading explains all three at once: the normal
        // one is additive in both paths, the mode 5 one is additive because GL_SetRenderMode
        // already made it so, and the mode 4 one wants ordinary alpha blending, which is
        // exactly where forcing ONE/ONE showed up as a difference. The reconstruction has
        // the comparison inverted.
        item.additive = (mesh_flags & goldsrc::STUDIO_NF_ADDITIVE) != 0 && rendermode == goldsrc::kRenderNormal;
        item.index_count = static_cast<GLsizei>(mesh.index_count);
        item.index_offset = static_cast<uintptr_t>(mesh.first_index) * sizeof(uint32_t);

        if (recording)
        {
            g_Submission.meshes++;
            g_Submission.shade_modes |= 1u << item.shade_mode;

            if (material != nullptr)
            {
                g_Submission.materials++;
            }
        }
    }

    // The model is done once every body part has reported in, which keeps the queue
    // inside the GL state scope the engine set up for this model.
    if (g_Pending.seen_parts >= g_Pending.expected_parts)
    {
        FlushPending();
    }

    return true;
}

bool render::Studio_TryDraw(goldsrc::model_t* model, const goldsrc::studiohdr_t* header, void* submodel)
{
    bool drawn = TryDrawSubmission(model, header, submodel);

    if (Dump_Armed())
    {
        StudioOutcome outcome;
        outcome.model = model != nullptr ? model->name : "";
        outcome.header = header != nullptr ? header->name : "";
        outcome.reason = drawn ? "" : Studio_LastRejection();
        outcome.drawn = drawn;

        Dump_Note(g_Submission, outcome);
    }

    return drawn;
}

const char* render::Studio_LastRejection()
{
    return g_LastRejection;
}

void render::Studio_WarmPrograms()
{
    std::vector<ProgramDesc> descs;
    descs.push_back(kProgram);

    for (const std::string& name : Shaders_ListPrograms())
    {
        // A material can only name a program that ships a manifest, so a pair without one
        // (the debug quad, the built-in whose desc is already in the list) is unreachable
        // through the material path and not worth a build.
        if (Manifest_Get(name) != nullptr)
        {
            descs.push_back(MaterialEntry(name).desc);
        }
    }

    Shaders_WarmUp(descs.data(), static_cast<int>(descs.size()));
}

void render::Studio_OnGammaChanged()
{
    g_LightGammaDirty = true;
}
