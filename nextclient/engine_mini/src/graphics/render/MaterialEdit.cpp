#include "engine.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <next_engine_mini/MaterialEditInterface.h>
#include <ncl_material/MaterialParse.h>
#include <ncl_material/MaterialResolve.h>
#include <ncl_material/ProgramManifest.h>
#include <ncl_material/MaterialPreset.h>

#include <ncl_math/vec3.h>

#include "common/cmd.h"
#include "common/common.h"
#include "common/filesystem.h"
#include "console/console.h"
#include "graphics/render/MaterialEdit.h"
#include "graphics/render/MaterialManifests.h"
#include "graphics/render/RenderConfig.h"
#include "graphics/render/ServerMaterials.h"
#include "graphics/render/ShaderManager.h"
#include "graphics/render/StudioIntercept.h"
#include "common/model.h"
#include "graphics/render/StudioGeometry.h"

namespace
{
    struct Session
    {
        bool active;
        std::string model;
        std::string texture;
        ncl_material::Material material;

        // Negative asks for nothing, which is what an author who has not touched the
        // switches is asking for.
        int shown_skin = -1;
        int shown_body = -1;
    };

    // Outlives the editing session on purpose: switching which texture is being edited must
    // not take the copy out of the world. The panel clears it when it closes, which is the
    // one moment after which nothing could.
    struct StandIn
    {
        std::string model;
        MaterialStandIn state = {};
        bool placement_requested = false;
    };

    StandIn g_StandIn;

    // Shown, and driven by the game, until a panel says otherwise. This is what the client
    // does when nobody is editing anything, and an all-zero struct says the opposite: it
    // would take the weapon out of every player's hands for the whole session.
    MaterialViewmodel g_Viewmodel = {true, false, {}};

    // Which model the panel has asked to be holding. Empty is the weapon the game gave the
    // author, which is what this is until somebody chooses otherwise.
    std::string g_ViewmodelPath;

    // The previous frame's answer, so the report can say whether anything moved rather than
    // only what it currently is.
    float g_ViewmodelFrame;
    float g_ViewmodelPosed;
    ncl_math::Vector3 g_ViewmodelBone;
    bool g_ViewmodelSampled;

    // The game's own weapon animation, kept for as long as the panel is driving instead.
    bool g_ViewmodelHeld;
    int g_HeldWeaponSequence;
    float g_HeldWeaponStartTime;

    // One list per target, in MaterialTarget order.
    std::string g_SequenceNames[static_cast<int>(MaterialTarget::Count)];

    MaterialAnimation* AnimationOf(MaterialTarget target)
    {
        switch (target)
        {
        case MaterialTarget::StandIn: return &g_StandIn.state.animation;
        case MaterialTarget::Viewmodel: return &g_Viewmodel.animation;
        default: return nullptr;
        }
    }

    std::string* NamesOf(MaterialTarget target)
    {
        int index = static_cast<int>(target);

        return index >= 0 && index < static_cast<int>(MaterialTarget::Count) ? &g_SequenceNames[index] : nullptr;
    }

    // What the docked panel covers, in its own coordinates. Both are needed: the ratio is
    // what survives the difference between vgui coordinates and frame pixels.
    int g_PanelWide;
    int g_ScreenWide;

    MaterialCamera g_Camera;
    bool g_PanelOpen;
    int g_AppliedInset;

    Session g_Session;

    // Storage behind the const char* the interface hands out. Each getter owns one, so a
    // caller holding two results at once does not find the first rewritten by the second.
    std::vector<std::string> g_Programs;
    std::string g_ValidationText;
    std::string g_Picked;

    // One line from the client about what it last did with all of this. The engine holds what
    // a panel asked for and the client applies it; a report of only the first half cannot say
    // which of the two lost it.
    std::string g_ClientReport;
    std::string g_ViewmodelDraw;
    std::string g_PresetStub;

    const char* const kEmpty = "";

    // The renderer's own ceiling for a path it will open, so the editor refuses what the
    // renderer would refuse instead of writing a document that draws nothing.
    constexpr size_t kMaxTexturePathLength = 128;

    const ncl_material::ProgramManifest* ManifestOf(const char* program)
    {
        if (program == nullptr || *program == '\0')
        {
            return nullptr;
        }

        return render::Manifest_Get(program);
    }

    const ncl_material::ParamGroup* GroupOf(const char* program, int group)
    {
        const ncl_material::ProgramManifest* manifest = ManifestOf(program);

        if (manifest == nullptr || group < 0 || group >= static_cast<int>(manifest->groups.size()))
        {
            return nullptr;
        }

        return &manifest->groups[group];
    }

    // Only what an author can actually choose. A program without a manifest can be neither
    // checked nor have its samplers enumerated, so both material sources refuse it; offering
    // one would be offering a choice the client goes on to reject.
    void RebuildPrograms()
    {
        g_Programs.clear();

        for (std::string& name : render::Shaders_ListPrograms())
        {
            if (render::Manifest_Get(name) != nullptr)
            {
                g_Programs.push_back(std::move(name));
            }
        }
    }

    ncl_material::Param* FindParam(const char* name)
    {
        if (!g_Session.active || name == nullptr)
        {
            return nullptr;
        }

        for (ncl_material::Param& param : g_Session.material.params)
        {
            if (param.name == name)
            {
                return &param;
            }
        }

        return nullptr;
    }

    // A material naming a program is only allowed the parameters that program declares, so
    // a value left over from the previous one would fail validation rather than be ignored.
    const ncl_material::ParamSchema* SchemaOf(const char* name)
    {
        const ncl_material::ProgramManifest* manifest = ManifestOf(g_Session.material.program.c_str());

        return manifest != nullptr && name != nullptr ? manifest->FindParam(name) : nullptr;
    }

    void SetParam(const char* name, const ncl_material::ParamValue& value)
    {
        if (ncl_material::Param* existing = FindParam(name))
        {
            existing->value = value;
            return;
        }

        g_Session.material.params.push_back(ncl_material::Param {name, value});
    }

    std::string DocumentPathOf(const std::string& model)
    {
        return model.size() > 4 ? model.substr(0, model.size() - 4) + ".mat" : std::string();
    }

    bool ReadFile(const std::string& path, std::string& out)
    {
        FileHandle_t file = FS_Open(path.c_str(), "rb");
        if (file == nullptr)
        {
            return false;
        }

        unsigned int size = FS_Size(file);
        out.resize(size);

        int read = size != 0 ? FS_Read(out.data(), static_cast<int>(size), file) : 0;
        FS_Close(file);

        return read == static_cast<int>(size);
    }

    // Written through the OS rather than the engine's filesystem, which offers no write
    // side here. The game directory is the mod directory, and a document sits next to its
    // model under it.
    bool WriteGameFile(const std::string& relative, const std::string& text, std::string& error)
    {
        std::filesystem::path path = std::filesystem::path(gEngfuncs.pfnGetGameDirectory()) / relative;

        std::error_code code;
        std::filesystem::create_directories(path.parent_path(), code);

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            error = "cannot open " + path.string();
            return false;
        }

        file.write(text.data(), static_cast<std::streamsize>(text.size()));

        if (!file)
        {
            error = "cannot write " + path.string();
            return false;
        }

        return true;
    }

    // How long one run of a sequence lasts, as the model declares it. Needed because the
    // engine poses the weapon from a moment in time rather than from a frame, so a frame has
    // to be expressed as one.
    float SequenceSeconds(model_t* model, int sequence)
    {
        const studiohdr_t* header = static_cast<const studiohdr_t*>(Mod_Extradata(model));

        if (header == nullptr || sequence < 0 || sequence >= header->numseq)
        {
            return 0.0f;
        }

        const mstudioseqdesc_t* description = reinterpret_cast<const mstudioseqdesc_t*>(
            reinterpret_cast<const uint8_t*>(header) + header->seqindex
        ) + sequence;

        if (description->fps <= 0.0f || description->numframes <= 1)
        {
            return 0.0f;
        }

        return static_cast<float>(description->numframes - 1) / description->fps;
    }

    // A model's own box, which is not in model_t: the studio loader leaves those bounds at
    // zero and keeps the real ones in the header. Some models fill only one of the two pairs
    // it can carry, and a few fill neither.
    void BoundsOf(model_t* model, ncl_math::Vector3& mins, ncl_math::Vector3& maxs)
    {
        constexpr float kAssumedHalfSize = 16.0f;

        mins = ncl_math::Vector3(-kAssumedHalfSize, -kAssumedHalfSize, -kAssumedHalfSize);
        maxs = ncl_math::Vector3(kAssumedHalfSize, kAssumedHalfSize, kAssumedHalfSize);

        const studiohdr_t* header = static_cast<const studiohdr_t*>(Mod_Extradata(model));
        if (header == nullptr)
        {
            return;
        }

        const float* low = header->bbmin;
        const float* high = header->bbmax;

        if (low[0] == high[0] && low[1] == high[1] && low[2] == high[2])
        {
            low = header->min;
            high = header->max;
        }

        if (low[0] == high[0] && low[1] == high[1] && low[2] == high[2])
        {
            return;
        }

        mins = ncl_math::Vector3::Ref(low);
        maxs = ncl_math::Vector3::Ref(high);
    }

    // The slab test, in the box's own space. Returns how far along the ray the box begins,
    // negative when the ray starts inside it, and false when it never enters.
    bool RayHitsBox(
        const ncl_math::Vector3& origin,
        const ncl_math::Vector3& direction,
        const ncl_math::Vector3& mins,
        const ncl_math::Vector3& maxs,
        float& distance)
    {
        float near_hit = -1e9f;
        float far_hit = 1e9f;

        for (int axis = 0; axis < 3; axis++)
        {
            float from = (&origin.x)[axis];
            float along = (&direction.x)[axis];
            float low = (&mins.x)[axis];
            float high = (&maxs.x)[axis];

            if (std::fabs(along) < 1e-6f)
            {
                if (from < low || from > high)
                {
                    return false;
                }

                continue;
            }

            float first = (low - from) / along;
            float second = (high - from) / along;

            if (first > second)
            {
                std::swap(first, second);
            }

            near_hit = std::max(near_hit, first);
            far_hit = std::min(far_hit, second);

            if (near_hit > far_hit)
            {
                return false;
            }
        }

        if (far_hit < 0.0f)
        {
            return false;
        }

        distance = near_hit;
        return true;
    }

    // Defined with the rest of the reporting, further down; the interface has to be able to
    // ask for it from up here.
    void WriteStateReport();

    class MaterialEdit : public MaterialEditInterface
    {
    public:
        int GetProgramCount() override
        {
            RebuildPrograms();

            return static_cast<int>(g_Programs.size());
        }

        const char* GetProgramName(int index) override
        {
            return index >= 0 && index < static_cast<int>(g_Programs.size()) ? g_Programs[index].c_str() : nullptr;
        }

        int GetParamCount(const char* program) override
        {
            const ncl_material::ProgramManifest* manifest = ManifestOf(program);

            return manifest != nullptr ? static_cast<int>(manifest->parameters.size()) : 0;
        }

        bool GetParam(const char* program, int index, MaterialEditParam* out) override
        {
            const ncl_material::ProgramManifest* manifest = ManifestOf(program);

            if (manifest == nullptr || out == nullptr || index < 0 ||
                index >= static_cast<int>(manifest->parameters.size()))
            {
                return false;
            }

            const ncl_material::ParamSchema& schema = manifest->parameters[index];

            out->name = schema.name.c_str();
            out->label = schema.label_or_name().c_str();
            out->hint = schema.hint.c_str();
            out->type = TypeOf(schema.type);
            out->widget = schema.widget == ncl_material::ParamWidget::Color
                ? MaterialEditParamWidget::Color
                : MaterialEditParamWidget::Default;
            out->instance = schema.instance;
            out->quantum = schema.quantum;
            out->has_range = schema.has_range;
            out->min_value = schema.min_value;
            out->max_value = schema.max_value;
            out->has_default = schema.has_default;
            out->default_scalar = schema.default_value.scalar;
            out->default_texture = schema.default_value.texture_ref.c_str();

            for (size_t i = 0; i < schema.default_value.vec4.size(); i++)
            {
                out->default_vec4[i] = schema.default_value.vec4[i];
            }

            return true;
        }

        int GetGroupCount(const char* program) override
        {
            const ncl_material::ProgramManifest* manifest = ManifestOf(program);

            return manifest != nullptr ? static_cast<int>(manifest->groups.size()) : 0;
        }

        const char* GetGroupLabel(const char* program, int group) override
        {
            const ncl_material::ParamGroup* found = GroupOf(program, group);

            return found != nullptr ? found->label_or_name().c_str() : kEmpty;
        }

        int GetGroupParamCount(const char* program, int group) override
        {
            const ncl_material::ParamGroup* found = GroupOf(program, group);

            return found != nullptr ? static_cast<int>(found->params.size()) : 0;
        }

        int GetGroupParam(const char* program, int group, int index) override
        {
            const ncl_material::ParamGroup* found = GroupOf(program, group);

            if (found == nullptr || index < 0 || index >= static_cast<int>(found->params.size()))
            {
                return -1;
            }

            return static_cast<int>(found->params[index]);
        }

        int GetModelCount() override
        {
            return render::Geometry_ModelCount();
        }

        const char* GetModelName(int index) override
        {
            return render::Geometry_ModelName(index);
        }

        int GetTextureCount(const char* model) override
        {
            return render::Geometry_TextureCount(model);
        }

        const char* GetTextureName(const char* model, int index) override
        {
            return render::Geometry_TextureName(model, index);
        }

        MaterialEditSource GetMaterialSource(const char* model, const char* texture_name) override
        {
            ncl_material::MaterialOrigin origin = ncl_material::MaterialOrigin::Implicit;

            if (render::Geometry_MaterialByName(model, texture_name, &origin) == nullptr)
            {
                return MaterialEditSource::None;
            }

            switch (origin)
            {
            case ncl_material::MaterialOrigin::Instance: return MaterialEditSource::Instance;
            case ncl_material::MaterialOrigin::ModelDocument: return MaterialEditSource::ModelDocument;
            default: return MaterialEditSource::Implicit;
            }
        }

        int GetSkinFamilyCount(const char* model) override
        {
            return render::Geometry_SkinFamilyCount(model);
        }

        int GetBodyCount(const char* model) override
        {
            return render::Geometry_BodyCount(model);
        }

        bool GetBodyName(const char* model, int body, char* out, int out_len) override
        {
            return render::Geometry_BodyName(model, body, out, out_len);
        }

        void SetShownAs(int skin_family, int body) override
        {
            g_Session.shown_skin = skin_family;
            g_Session.shown_body = body;
        }

        int GetShownSkin() override
        {
            return g_Session.shown_skin;
        }

        int GetShownBody() override
        {
            return g_Session.shown_body;
        }

        void SetStandInModel(const char* path) override
        {
            std::string wanted = path != nullptr ? path : "";

            // Only a real change drops the animations. The panel pushes the whole stand-in
            // state, model included, every time anything on the page is touched; clearing
            // unconditionally emptied the list on every one of those, and the client only
            // republishes when the model it has loaded is not the one being asked for.
            if (wanted == g_StandIn.model)
            {
                return;
            }

            g_StandIn.model = wanted;

            // Dropped here rather than left for the client to overwrite: between this and the
            // client loading the new model, the names still standing would be the old
            // model's, and the panel would offer animations that no longer exist.
            g_SequenceNames[static_cast<int>(MaterialTarget::StandIn)].clear();
            g_StandIn.state.animation.sequence_count = 0;
        }

        const char* GetStandInModel() override
        {
            return g_StandIn.model.c_str();
        }

        void SetViewmodelModel(const char* path) override
        {
            std::string wanted = path != nullptr ? path : "";

            // Same reason as the stand-in: the panel pushes the whole page on every change,
            // and dropping the animations on each of those would empty the list under the
            // author's cursor.
            if (wanted == g_ViewmodelPath)
            {
                return;
            }

            g_ViewmodelPath = wanted;

            g_SequenceNames[static_cast<int>(MaterialTarget::Viewmodel)].clear();
            g_Viewmodel.animation.sequence_count = 0;
        }

        const char* GetViewmodelModel() override
        {
            return g_ViewmodelPath.c_str();
        }

        void GetStandIn(MaterialStandIn* out) override
        {
            if (out != nullptr)
            {
                *out = g_StandIn.state;
            }
        }

        void SetStandIn(const MaterialStandIn* state) override
        {
            if (state != nullptr)
            {
                g_StandIn.state = *state;
            }
        }

        // What a click over the scene landed on. Walks what the renderer was last given to
        // draw rather than what the map holds: an author points at something they can see,
        // and being drawn is what makes a model editable in the first place.
        const char* PickModel(float u, float v) override
        {
            if (r_refdef == nullptr || p_cl_visedicts == nullptr || p_cl_numvisedicts == nullptr)
            {
                return kEmpty;
            }

            ncl_math::Vector3 forward;
            ncl_math::Vector3 right;
            ncl_math::Vector3 up;
            ncl_math::AngleVectors(r_refdef->viewangles, &forward.x, &right.x, &up.x);

            // The same projection the frame was drawn with: the horizontal half-angle from
            // the engine's own field of view, and the vertical one from the shape of the
            // rectangle the world went into, which is not the shape of the screen while a
            // panel is docked over part of it.
            float fov_x = p_scr_fov_value != nullptr && *p_scr_fov_value > 1.0f ? *p_scr_fov_value : 90.0f;
            float tan_x = std::tan(fov_x * 0.5f * 3.14159265f / 180.0f);
            float aspect = r_refdef->vrect.height > 0
                ? static_cast<float>(r_refdef->vrect.width) / static_cast<float>(r_refdef->vrect.height)
                : 1.0f;

            ncl_math::Vector3 direction =
                forward + right * ((u * 2.0f - 1.0f) * tan_x) + up * ((1.0f - v * 2.0f) * (tan_x / aspect));

            direction = direction.Normalized();

            ncl_math::Vector3 eye = ncl_math::Vector3::Ref(r_refdef->vieworg);

            g_Picked.clear();
            float nearest = 1e9f;

            for (int i = 0; i < *p_cl_numvisedicts; i++)
            {
                cl_entity_t* entity = p_cl_visedicts[i];

                if (entity == nullptr || entity->model == nullptr || entity->model->type != mod_studio)
                {
                    continue;
                }

                ncl_math::Vector3 entity_forward;
                ncl_math::Vector3 entity_right;
                ncl_math::Vector3 entity_up;
                ncl_math::AngleVectors(entity->angles, &entity_forward.x, &entity_right.x, &entity_up.x);

                // Into the model's own space, where its box is axis-aligned. Right is negated
                // because the engine's right-hand vector points the other way from the axis
                // the box is measured along.
                ncl_math::Vector3 delta = eye - ncl_math::Vector3::Ref(entity->origin);

                ncl_math::Vector3 local_origin(delta.Dot(entity_forward), -delta.Dot(entity_right), delta.Dot(entity_up));
                ncl_math::Vector3 local_direction(
                    direction.Dot(entity_forward), -direction.Dot(entity_right), direction.Dot(entity_up));

                ncl_math::Vector3 mins;
                ncl_math::Vector3 maxs;
                BoundsOf(entity->model, mins, maxs);

                float distance = 0.0f;

                if (RayHitsBox(local_origin, local_direction, mins, maxs, distance) && distance < nearest)
                {
                    nearest = distance;
                    g_Picked = entity->model->name;
                }
            }

            return g_Picked.c_str();
        }

        void DumpState() override
        {
            WriteStateReport();
        }

        void SetClientReport(const char* text) override
        {
            g_ClientReport = text != nullptr ? text : "";
        }

        void SetPanelOpen(bool open) override
        {
            g_PanelOpen = open;
        }

        bool IsPanelOpen() override
        {
            return g_PanelOpen;
        }

        void SetViewportInset(int panel_wide, int screen_wide) override
        {
            g_PanelWide = panel_wide;
            g_ScreenWide = screen_wide;
        }

        int GetAppliedInset() override
        {
            return g_AppliedInset;
        }

        void GetCamera(MaterialCamera* out) override
        {
            if (out != nullptr)
            {
                *out = g_Camera;
            }
        }

        void SetCamera(const MaterialCamera* camera) override
        {
            if (camera != nullptr)
            {
                g_Camera = *camera;
            }
        }

        void GetViewmodel(MaterialViewmodel* out) override
        {
            if (out != nullptr)
            {
                *out = g_Viewmodel;
            }
        }

        void SetViewmodel(const MaterialViewmodel* viewmodel) override
        {
            if (viewmodel != nullptr)
            {
                g_Viewmodel = *viewmodel;
            }
        }

        void GetAnimation(MaterialTarget target, MaterialAnimation* out) override
        {
            const MaterialAnimation* animation = AnimationOf(target);

            if (out != nullptr && animation != nullptr)
            {
                *out = *animation;
            }
        }

        void SetAnimation(MaterialTarget target, const MaterialAnimation* animation) override
        {
            MaterialAnimation* stored = AnimationOf(target);

            if (animation != nullptr && stored != nullptr)
            {
                *stored = *animation;
            }
        }

        void SetSequenceNames(MaterialTarget target, const char* names) override
        {
            if (std::string* stored = NamesOf(target))
            {
                *stored = names != nullptr ? names : "";
            }
        }

        const char* GetSequenceNames(MaterialTarget target) override
        {
            const std::string* stored = NamesOf(target);

            return stored != nullptr ? stored->c_str() : kEmpty;
        }

        void RequestStandInPlacement() override
        {
            g_StandIn.placement_requested = true;
        }

        bool TakeStandInPlacement() override
        {
            bool requested = g_StandIn.placement_requested;
            g_StandIn.placement_requested = false;

            return requested;
        }

        bool Begin(const char* model, const char* texture_name) override
        {
            const ncl_material::Material* current = render::Geometry_MaterialByName(model, texture_name, nullptr);
            if (current == nullptr)
            {
                return false;
            }

            g_Session.active = true;
            g_Session.model = model;
            g_Session.texture = texture_name;
            g_Session.material = *current;

            return true;
        }

        void End() override
        {
            g_Session = Session {};
        }

        bool IsEditing() override
        {
            return g_Session.active;
        }

        const char* GetEditedModel() override
        {
            return g_Session.active ? g_Session.model.c_str() : kEmpty;
        }

        const char* GetEditedTexture() override
        {
            return g_Session.active ? g_Session.texture.c_str() : kEmpty;
        }

        const char* GetEditedProgram() override
        {
            return g_Session.active ? g_Session.material.program.c_str() : kEmpty;
        }

        bool SetEditedProgram(const char* program) override
        {
            if (!g_Session.active || program == nullptr || *program == '\0')
            {
                return false;
            }

            // The legacy program is what a model with no material of its own resolves to,
            // so naming it is how an author says the material should do nothing.
            const ncl_material::ProgramManifest* manifest = ManifestOf(program);
            bool legacy = program == std::string_view(ncl_material::kLegacyProgram);

            if (manifest == nullptr && !legacy)
            {
                return false;
            }

            g_Session.material.program = program;

            std::vector<ncl_material::Param> kept;

            for (ncl_material::Param& param : g_Session.material.params)
            {
                const ncl_material::ParamSchema* schema =
                    manifest != nullptr ? manifest->FindParam(param.name) : nullptr;

                if (schema != nullptr && schema->type == param.value.type)
                {
                    kept.push_back(std::move(param));
                }
            }

            g_Session.material.params = std::move(kept);

            if (manifest != nullptr)
            {
                ncl_material::ApplyDefaults(g_Session.material, *manifest);
            }

            return true;
        }

        bool GetScalar(const char* name, float* out) override
        {
            const ncl_material::Param* param = FindParam(name);

            if (param == nullptr || out == nullptr || param->value.type != ncl_material::ParamType::Scalar)
            {
                return false;
            }

            *out = param->value.scalar;

            return true;
        }

        bool GetVec4(const char* name, float* out) override
        {
            const ncl_material::Param* param = FindParam(name);

            if (param == nullptr || out == nullptr || param->value.type != ncl_material::ParamType::Vec4)
            {
                return false;
            }

            for (size_t i = 0; i < param->value.vec4.size(); i++)
            {
                out[i] = param->value.vec4[i];
            }

            return true;
        }

        const char* GetTexture(const char* name) override
        {
            const ncl_material::Param* param = FindParam(name);

            if (param == nullptr || param->value.type != ncl_material::ParamType::TextureRef)
            {
                return kEmpty;
            }

            return param->value.texture_ref.c_str();
        }

        bool SetScalar(const char* name, float value) override
        {
            const ncl_material::ParamSchema* schema = SchemaOf(name);

            if (schema == nullptr || schema->type != ncl_material::ParamType::Scalar)
            {
                return false;
            }

            ncl_material::ParamValue stored;
            stored.type = ncl_material::ParamType::Scalar;

            // Snapped and clamped rather than refused, unlike an authored file: the grid is
            // what bounds how many distinct bakes exist, and a slider that produced an
            // off-grid or out-of-range value would write a document the client then rejects.
            stored.scalar = schema->quantum > 0.0f
                ? std::round(value / schema->quantum) * schema->quantum
                : value;

            if (schema->has_range)
            {
                stored.scalar = std::clamp(stored.scalar, schema->min_value, schema->max_value);
            }

            SetParam(name, stored);

            return true;
        }

        bool SetVec4(const char* name, const float* value) override
        {
            const ncl_material::ParamSchema* schema = SchemaOf(name);

            if (schema == nullptr || value == nullptr || schema->type != ncl_material::ParamType::Vec4)
            {
                return false;
            }

            ncl_material::ParamValue stored;
            stored.type = ncl_material::ParamType::Vec4;

            for (size_t i = 0; i < stored.vec4.size(); i++)
            {
                // Held inside the declared range rather than refused, as a scalar is: a
                // typed number outside it would otherwise be kept until saving and then
                // rejected, with the model in the meantime showing a value the program
                // never allowed.
                stored.vec4[i] = schema->has_range
                    ? std::clamp(value[i], schema->min_value, schema->max_value)
                    : value[i];
            }

            SetParam(name, stored);

            return true;
        }

        bool SetTexture(const char* name, const char* path) override
        {
            const ncl_material::ParamSchema* schema = SchemaOf(name);

            if (schema == nullptr || path == nullptr || schema->type != ncl_material::ParamType::TextureRef)
            {
                return false;
            }

            // An empty path clears the reference instead of writing one nothing can open.
            if (*path == '\0')
            {
                return ClearParam(name);
            }

            // Checked here so the panel never has to know the rule: the same one the
            // renderer applies before it opens a material's image, refused at the point
            // where an author can still see why.
            if (!ncl_material::IsAdmissibleTexturePath(path, kMaxTexturePathLength))
            {
                return false;
            }

            ncl_material::ParamValue stored;
            stored.type = ncl_material::ParamType::TextureRef;
            stored.texture_ref = path;

            SetParam(name, stored);

            return true;
        }

        bool ClearParam(const char* name) override
        {
            if (!g_Session.active || name == nullptr)
            {
                return false;
            }

            for (size_t i = 0; i < g_Session.material.params.size(); i++)
            {
                if (g_Session.material.params[i].name == name)
                {
                    g_Session.material.params.erase(g_Session.material.params.begin() + i);
                    return true;
                }
            }

            return false;
        }

        const char* Validate() override
        {
            g_ValidationText.clear();

            if (!g_Session.active)
            {
                return kEmpty;
            }

            if (g_Session.material.program == ncl_material::kLegacyProgram)
            {
                return kEmpty;
            }

            const ncl_material::ProgramManifest* manifest = ManifestOf(g_Session.material.program.c_str());

            if (manifest == nullptr)
            {
                g_ValidationText = "no program \"" + g_Session.material.program + "\"";
                return g_ValidationText.c_str();
            }

            ncl_utils::Result<> result = ncl_material::Validate(g_Session.material, *manifest);

            if (result.has_error())
            {
                g_ValidationText = result.error_str();
            }

            return g_ValidationText.c_str();
        }

        bool Save(const char* model, char* error_out, int error_out_len) override
        {
            std::string error;
            bool ok = SaveTo(model != nullptr ? model : "", error);

            if (!ok && error_out != nullptr && error_out_len > 0)
            {
                V_strncpy(error_out, error.c_str(), error_out_len);
            }

            return ok;
        }

        const char* GetPresetStub() override
        {
            g_PresetStub.clear();

            if (!g_Session.active)
            {
                return kEmpty;
            }

            // Named after the texture it dresses, which is a name an author recognises and
            // a plugin can be written against; the preset it goes into is the author's to
            // choose, so only the one material is written.
            ncl_material::PresetMaterial material;
            material.name = g_Session.texture;
            material.texture_name = g_Session.texture;
            material.material = g_Session.material;

            g_PresetStub = ncl_material::WritePresetMaterial(material);

            return g_PresetStub.c_str();
        }

        void Reload() override
        {
            render::Geometry_ReloadDocuments();
        }

    private:
        static MaterialEditParamType TypeOf(ncl_material::ParamType type)
        {
            switch (type)
            {
            case ncl_material::ParamType::Vec4: return MaterialEditParamType::Vec4;
            case ncl_material::ParamType::TextureRef: return MaterialEditParamType::Texture;
            default: return MaterialEditParamType::Scalar;
            }
        }

        bool SaveTo(const std::string& model, std::string& error)
        {
            if (!g_Session.active)
            {
                error = "nothing is being edited";
                return false;
            }

            std::string target = model.empty() ? g_Session.model : model;
            std::string path = DocumentPathOf(target);

            if (path.empty())
            {
                error = "\"" + target + "\" is not a model path";
                return false;
            }

            // Read first: the document describes every texture of the model, and rewriting
            // it from the one texture under edit would drop the others.
            ncl_material::MaterialDoc doc;
            std::string existing;

            if (ReadFile(path, existing))
            {
                ncl_utils::ResultT<ncl_material::MaterialDoc> parsed = ncl_material::ParseMaterialDoc(existing);

                if (parsed.has_error())
                {
                    // Refused rather than overwritten: whatever is in there was authored by
                    // somebody, and a save is not the moment to discard it.
                    error = path + ": " + parsed.error_str();
                    return false;
                }

                doc = *parsed;
            }

            bool replaced = false;

            for (ncl_material::MaterialEntry& entry : doc.entries)
            {
                if (ncl_material::EqualsIgnoreCaseAscii(entry.texture_name, g_Session.texture))
                {
                    entry.material = g_Session.material;
                    replaced = true;
                    break;
                }
            }

            if (!replaced)
            {
                doc.entries.push_back(ncl_material::MaterialEntry {g_Session.texture, g_Session.material});
            }

            if (!WriteGameFile(path, ncl_material::WriteMaterialDoc(doc), error))
            {
                return false;
            }

            render::Geometry_ReloadDocuments();

            return true;
        }
    };

    MaterialEdit g_Interface;

    const char* SourceName(MaterialEditSource source)
    {
        switch (source)
        {
        case MaterialEditSource::Instance: return "server binding";
        case MaterialEditSource::ModelDocument: return "model document";
        case MaterialEditSource::Implicit: return "implicit";
        default: return "none";
        }
    }

    void PrintUsage()
    {
        Con_Printf("r_mat_edit models                   models prepared this map\n");
        Con_Printf("r_mat_edit textures <model>         texture names and where each material comes from\n");
        Con_Printf("r_mat_edit begin <model> <texture>  start editing, seeded from what is on screen\n");
        Con_Printf("r_mat_edit end                      drop the editor's layer\n");
        Con_Printf("r_mat_edit program <name>           switch program, taking its defaults\n");
        Con_Printf("r_mat_edit show                     the schema and the current values\n");
        Con_Printf("r_mat_edit set <param> <value...>   a number, four numbers, or a texture path\n");
        Con_Printf("r_mat_edit clear <param>\n");
        Con_Printf("r_mat_edit save [model]             writes <model>.mat, defaulting to the edited one\n");
        Con_Printf("r_mat_edit stub                     the same material as a preset entry\n");
        Con_Printf("r_mat_edit reload                   re-read the documents beside the models\n");
    Con_Printf("r_mat_state                         what the editor currently believes\n");
    }

    void PrintPrograms()
    {
        Con_Printf("programs with a manifest:\n");

        for (int i = 0; i < g_Interface.GetProgramCount(); i++)
        {
            Con_Printf("  %s\n", g_Interface.GetProgramName(i));
        }
    }

    void PrintModels()
    {
        int count = g_Interface.GetModelCount();

        for (int i = 0; i < count; i++)
        {
            Con_Printf("  %s\n", g_Interface.GetModelName(i));
        }

        Con_Printf("%d models\n", count);

        if (count != 0)
        {
            return;
        }

        // The shader path is what prepares studio models, it is off by default and it is
        // not saved with the configuration, so it is off again after every restart. An
        // empty list that does not say which of the two reasons it is costs an evening.
        bool shader_path_on = render::g_RenderCfg.studio_shader != nullptr &&
            render::g_RenderCfg.studio_shader->value != 0.0f;

        Con_Printf(shader_path_on
            ? "r_mat_edit: nothing has been drawn through the shader path yet; look at a model first\n"
            : "r_mat_edit: r_studio_shader is 0, so no model has a material at all; set it to 1 and look at a model\n");
    }

    void PrintTextures(const char* model)
    {
        int count = g_Interface.GetTextureCount(model);

        for (int i = 0; i < count; i++)
        {
            const char* name = g_Interface.GetTextureName(model, i);
            Con_Printf("  %-24s %s\n", name, SourceName(g_Interface.GetMaterialSource(model, name)));
        }

        if (count == 0)
        {
            Con_Printf("r_mat_edit: %s is not prepared; look at it once first\n", model);
        }
    }

    void PrintValue(const MaterialEditParam& param)
    {
        if (param.type == MaterialEditParamType::Scalar)
        {
            float value = 0.0f;
            if (g_Interface.GetScalar(param.name, &value))
            {
                Con_Printf(" = %.4f", value);
            }

            return;
        }

        if (param.type == MaterialEditParamType::Vec4)
        {
            float value[4] = {};
            if (g_Interface.GetVec4(param.name, value))
            {
                Con_Printf(" = %.4f %.4f %.4f %.4f", value[0], value[1], value[2], value[3]);
            }

            return;
        }

        const char* path = g_Interface.GetTexture(param.name);
        if (*path != '\0')
        {
            Con_Printf(" = %s", path);
        }
    }

    void PrintSchema()
    {
        if (!g_Interface.IsEditing())
        {
            Con_Printf("r_mat_edit: nothing is being edited\n");
            return;
        }

        const char* program = g_Interface.GetEditedProgram();

        Con_Printf("%s, texture %s\n", g_Interface.GetEditedModel(), g_Interface.GetEditedTexture());
        Con_Printf("program %s, underlying source %s\n", program,
            SourceName(g_Interface.GetMaterialSource(g_Interface.GetEditedModel(), g_Interface.GetEditedTexture())));

        for (int group = 0; group < g_Interface.GetGroupCount(program); group++)
        {
            const char* label = g_Interface.GetGroupLabel(program, group);
            Con_Printf("  [%s]\n", *label != '\0' ? label : "-");

            for (int i = 0; i < g_Interface.GetGroupParamCount(program, group); i++)
            {
                MaterialEditParam param;
                if (!g_Interface.GetParam(program, g_Interface.GetGroupParam(program, group, i), &param))
                {
                    continue;
                }

                Con_Printf("    %-20s %s", param.name, param.instance ? "instance" : "        ");
                PrintValue(param);
                Con_Printf("\n");
            }
        }

        const char* problem = g_Interface.Validate();
        Con_Printf(*problem != '\0' ? "invalid: %s\n" : "valid%s\n", problem);
    }

    void SetFromArguments()
    {
        const char* name = Cmd_Argv(2);
        int values = Cmd_Argc() - 3;

        if (values == 1)
        {
            const char* text = Cmd_Argv(3);
            char* end = nullptr;
            float number = static_cast<float>(strtod(text, &end));

            bool ok = end != text && *end == '\0'
                ? g_Interface.SetScalar(name, number)
                : g_Interface.SetTexture(name, text);

            if (!ok)
            {
                Con_Printf("r_mat_edit: \"%s\" is not a parameter of this program, or is of another type\n", name);
            }

            return;
        }

        if (values == 4)
        {
            float vector[4];
            for (int i = 0; i < 4; i++)
            {
                vector[i] = static_cast<float>(atof(Cmd_Argv(3 + i)));
            }

            if (!g_Interface.SetVec4(name, vector))
            {
                Con_Printf("r_mat_edit: \"%s\" is not a vec4 parameter of this program\n", name);
            }

            return;
        }

        Con_Printf("r_mat_edit set <param> <number|path|four numbers>\n");
    }

    std::string g_State;

    void StateLine(const char* format, ...)
    {
        char line[512];

        va_list args;
        va_start(args, format);
        Q_vsnprintf(line, sizeof(line), format, args);
        va_end(args);

        g_State += line;
    }

    const char* YesNo(bool value)
    {
        return value ? "yes" : "no";
    }

    int LineCount(const std::string& text)
    {
        return text.empty() ? 0 : static_cast<int>(std::count(text.begin(), text.end(), '\n')) + 1;
    }

    void PrintAnimation(const char* what, const MaterialAnimation& animation, MaterialTarget target)
    {
        StateLine(
            "  %-10s sequence %d of %d, frame %.3f, playing %s, names %d\n",
            what,
            animation.sequence,
            animation.sequence_count,
            animation.frame,
            YesNo(animation.playing),
            LineCount(*NamesOf(target))
        );
    }

    // Everything the editor decides about the running frame, printed as it actually stands.
    // Not a convenience: the panel cannot be watched from outside, so a report of what it
    // believes is the difference between a fix and a guess.
    void BuildState()
    {
        g_State.clear();

        StateLine("panel      open %s, covering %d of %d\n", YesNo(g_PanelOpen), g_PanelWide, g_ScreenWide);
        StateLine(
            "camera     active %s, waiting for a start %s, at %.0f %.0f %.0f, looking %.0f %.0f\n",
            YesNo(g_Camera.active),
            YesNo(g_Camera.seed_requested),
            g_Camera.origin[0], g_Camera.origin[1], g_Camera.origin[2],
            g_Camera.angles[0], g_Camera.angles[1]
        );

        if (r_refdef != nullptr)
        {
            StateLine(
                "view       %d x %d at %d %d, fov %.1f, taken off the world last frame %d\n",
                r_refdef->vrect.width, r_refdef->vrect.height, r_refdef->vrect.x, r_refdef->vrect.y,
                p_scr_fov_value != nullptr ? *p_scr_fov_value : 0.0f,
                g_AppliedInset
            );
        }

        StateLine(
            "session    editing %s, %s : %s, program %s, shown as skin %d body %d\n",
            YesNo(g_Session.active),
            g_Session.active ? g_Session.model.c_str() : "-",
            g_Session.active ? g_Session.texture.c_str() : "-",
            g_Session.active ? g_Session.material.program.c_str() : "-",
            g_Session.shown_skin,
            g_Session.shown_body
        );

        StateLine(
            "stand-in   %s, placement pending %s, spinning %s, scale %.2f\n",
            g_StandIn.model.empty() ? "-" : g_StandIn.model.c_str(),
            YesNo(g_StandIn.placement_requested),
            YesNo(g_StandIn.state.spinning),
            g_StandIn.state.scale
        );
        PrintAnimation("", g_StandIn.state.animation, MaterialTarget::StandIn);

        StateLine(
            "viewmodel  shown %s, driven by the panel %s, asked for %s\n",
            YesNo(g_Viewmodel.shown),
            YesNo(g_Viewmodel.overriding),
            g_ViewmodelPath.empty() ? "the game's own" : g_ViewmodelPath.c_str()
        );
        PrintAnimation("", g_Viewmodel.animation, MaterialTarget::Viewmodel);

        StateLine("client     %s\n", g_ClientReport.empty() ? "silent" : g_ClientReport.c_str());
        StateLine("at draw    %s\n", g_ViewmodelDraw.empty() ? "the hands were not drawn" : g_ViewmodelDraw.c_str());

        // The two that explain an empty model list, which is the question this gets asked for
        // more than any other.
        StateLine(
            "renderer   r_studio_shader %.0f, prepared models %d, programs %d\n",
            render::g_RenderCfg.studio_shader != nullptr ? render::g_RenderCfg.studio_shader->value : -1.0f,
            g_Interface.GetModelCount(),
            g_Interface.GetProgramCount()
        );
    }

    // Written beside the game rather than only printed. The console is a panel of the game
    // ui, the game ui is hidden while the editor is up, and a report nobody can read is not
    // a report.
    void WriteStateReport()
    {
        BuildState();

        Con_Printf("%s", g_State.c_str());

        std::string error;

        if (!WriteGameFile("materials_state.txt", g_State, error))
        {
            Con_Printf("r_mat_state: %s\n", error.c_str());
        }
    }

    void MaterialState_f()
    {
        WriteStateReport();
    }

    void MaterialEdit_f()
    {
        if (Cmd_Argc() < 2)
        {
            PrintUsage();

            if (g_Interface.IsEditing())
            {
                Con_Printf("\n");
                PrintSchema();
            }

            return;
        }

        const char* verb = Cmd_Argv(1);

        if (V_stricmp(verb, "models") == 0)
        {
            PrintModels();
            return;
        }

        if (V_stricmp(verb, "programs") == 0)
        {
            PrintPrograms();
            return;
        }

        if (V_stricmp(verb, "textures") == 0 && Cmd_Argc() >= 3)
        {
            PrintTextures(Cmd_Argv(2));
            return;
        }

        if (V_stricmp(verb, "begin") == 0 && Cmd_Argc() >= 4)
        {
            if (!g_Interface.Begin(Cmd_Argv(2), Cmd_Argv(3)))
            {
                Con_Printf("r_mat_edit: no such prepared model or texture\n");
                return;
            }

            PrintSchema();
            return;
        }

        if (V_stricmp(verb, "end") == 0)
        {
            g_Interface.End();
            Con_Printf("r_mat_edit: editing ended, the model is back on its own material\n");
            return;
        }

        if (V_stricmp(verb, "program") == 0 && Cmd_Argc() >= 3)
        {
            if (!g_Interface.SetEditedProgram(Cmd_Argv(2)))
            {
                Con_Printf("r_mat_edit: no program \"%s\" with a manifest\n", Cmd_Argv(2));
                return;
            }

            PrintSchema();
            return;
        }

        if (V_stricmp(verb, "show") == 0)
        {
            PrintSchema();
            return;
        }

        if (V_stricmp(verb, "set") == 0 && Cmd_Argc() >= 4)
        {
            SetFromArguments();
            return;
        }

        if (V_stricmp(verb, "clear") == 0 && Cmd_Argc() >= 3)
        {
            if (!g_Interface.ClearParam(Cmd_Argv(2)))
            {
                Con_Printf("r_mat_edit: \"%s\" is not set\n", Cmd_Argv(2));
            }

            return;
        }

        if (V_stricmp(verb, "save") == 0)
        {
            char error[256] = {};

            if (!g_Interface.Save(Cmd_Argc() >= 3 ? Cmd_Argv(2) : nullptr, error, sizeof(error)))
            {
                Con_Printf("r_mat_edit: %s\n", error);
                return;
            }

            Con_Printf("r_mat_edit: wrote the document; \"r_mat_edit end\" to see it without the editor's layer\n");
            return;
        }

        if (V_stricmp(verb, "stub") == 0)
        {
            Con_Printf("%s\n", g_Interface.GetPresetStub());
            return;
        }

        if (V_stricmp(verb, "reload") == 0)
        {
            g_Interface.Reload();
            Con_Printf("r_mat_edit: documents will be re-read\n");
            return;
        }

        PrintUsage();
    }
}

const ncl_material::Material* render::MaterialEdit_Override(std::string_view model, std::string_view texture_name)
{
    if (!g_Session.active || g_Session.model != model ||
        !ncl_material::EqualsIgnoreCaseAscii(g_Session.texture, texture_name))
    {
        return nullptr;
    }

    return &g_Session.material;
}

int render::MaterialEdit_ShownSkin(std::string_view model)
{
    return g_Session.active && g_Session.model == model ? g_Session.shown_skin : -1;
}

int render::MaterialEdit_ShownBody(std::string_view model)
{
    return g_Session.active && g_Session.model == model ? g_Session.shown_body : -1;
}

bool render::MaterialEdit_DrivesViewmodel()
{
    return g_PanelOpen && g_Viewmodel.overriding;
}

void render::MaterialEdit_ApplyViewmodel(cl_entity_t* entity, float now, int* weapon_sequence, float* weapon_start_time)
{
    if (weapon_sequence == nullptr || weapon_start_time == nullptr)
    {
        return;
    }

    if (!MaterialEdit_DrivesViewmodel())
    {
        // Put back the moment the panel lets go. These two belong to the game, and a weapon
        // left on the panel's frame would stand there until the next animation the game
        // happens to play.
        if (g_ViewmodelHeld)
        {
            *weapon_sequence = g_HeldWeaponSequence;
            *weapon_start_time = g_HeldWeaponStartTime;
            g_ViewmodelHeld = false;
        }

        return;
    }

    if (!g_ViewmodelHeld)
    {
        g_HeldWeaponSequence = *weapon_sequence;
        g_HeldWeaponStartTime = *weapon_start_time;
        g_ViewmodelHeld = true;
    }

    const MaterialAnimation& animation = g_Viewmodel.animation;

    // Written to the client state, not to the entity. R_DrawViewModel rewrites the whole
    // animation of the view entity from these two immediately before it draws it - frame to
    // zero, sequence and animtime to these - so anything written on the entity is discarded
    // unread, however late it is written.
    //
    // What the renderer then estimates is (now - start) * fps, and the frame it lands on is
    // therefore chosen by placing the start that many seconds in the past.
    *weapon_sequence = animation.sequence;
    *weapon_start_time = now - animation.frame * SequenceSeconds(entity != nullptr ? entity->model : nullptr, animation.sequence);
}

void render::MaterialEdit_NoteViewmodel(const cl_entity_t* entity, float now)
{
    if (!g_PanelOpen || entity == nullptr)
    {
        return;
    }

    // The first bone's translation, which is what a moving pose moves. Taken because a frame
    // that advances proves only that the number changed: whether the model followed it is a
    // separate question, and this is the answer to it.
    const engine_studio_api_s* api = render::Studio_OriginalApi();
    const float* bones = api != nullptr && api->StudioGetBoneTransform != nullptr
        ? reinterpret_cast<const float*>(api->StudioGetBoneTransform())
        : nullptr;

    ncl_math::Vector3 bone;

    if (bones != nullptr)
    {
        // Three rows of four, translation in the last column of each.
        bone = ncl_math::Vector3 {bones[3], bones[7], bones[11]};
    }

    // What the renderer settled on, as it wrote it back: R_StudioSetupBones ends by storing
    // the frame it posed the model at, so this is its answer rather than our request.
    float posed = entity->latched.prevframe;

    float frame_moved = g_ViewmodelSampled ? entity->curstate.frame - g_ViewmodelFrame : 0.0f;
    float posed_moved = g_ViewmodelSampled ? posed - g_ViewmodelPosed : 0.0f;
    float bone_moved = g_ViewmodelSampled ? (bone - g_ViewmodelBone).Length() : 0.0f;

    g_ViewmodelFrame = entity->curstate.frame;
    g_ViewmodelPosed = posed;
    g_ViewmodelBone = bone;
    g_ViewmodelSampled = true;

    char line[320];
    Q_snprintf(
        line, sizeof(line),
        "%s, sequence %d, frame %.1f, animtime %.2f, rate %.2f, now %.2f, posed at %.1f; "
        "since the frame before: frame %+.2f, posed %+.2f, first bone %.3f",
        entity->model != nullptr ? entity->model->name : "no model",
        entity->curstate.sequence,
        entity->curstate.frame,
        entity->curstate.animtime,
        entity->curstate.framerate,
        now,
        posed,
        frame_moved,
        posed_moved,
        bone_moved
    );

    g_ViewmodelDraw = line;
}

int render::MaterialEdit_ViewportInset(int frame_wide)
{
    if (g_PanelWide <= 0 || g_ScreenWide <= 0 || frame_wide <= 0)
    {
        return 0;
    }

    int inset = frame_wide * g_PanelWide / g_ScreenWide;

    // A frame narrower than this is not a view of anything, and a viewport of zero width is
    // a division by zero in the projection.
    constexpr int kMinimumViewWide = 64;

    return std::min(inset, std::max(frame_wide - kMinimumViewWide, 0));
}

void render::MaterialEdit_SetAppliedInset(int pixels)
{
    g_AppliedInset = pixels;
}

int render::MaterialEdit_AppliedInset()
{
    return g_AppliedInset;
}

MaterialEditInterface* render::MaterialEdit_Get()
{
    return &g_Interface;
}

void render::MaterialEdit_RegisterCommands()
{
    Cmd_AddCommand("r_mat_edit", MaterialEdit_f);
    Cmd_AddCommand("r_mat_state", MaterialState_f);
}
