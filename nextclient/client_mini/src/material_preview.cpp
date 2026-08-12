#include "material_preview.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <next_engine_mini/MaterialEditInterface.h>

// main.h first: studio.h uses vec3_t without declaring it, and the model structures come
// in through there.
#include "main.h"
#include <entity_types.h>
#include <studio.h>

namespace
{
    // Far enough that a weapon is seen the way a player across a room sees it, near enough
    // that it is not a speck. The author flies in from here.
    constexpr float kPlacementDistance = 96.0f;

    // Measured down from the eye rather than up from anything, so the free camera and the
    // player's own view put it in the same place. On the eye it sits on the crosshair; a
    // head lower is where a weapon is carried.
    constexpr float kPlacementDrop = 20.0f;

    // A player's origin is the centre of its hull, not its feet, and its eye is this far
    // above it.
    constexpr float kEyeHeight = 28.0f;

    constexpr float kSpinDegreesPerSecond = 30.0f;

    // What a sequence lasts when the model does not say. Only reached by a model whose
    // header declares no rate, which is a broken model rather than a normal one.
    constexpr float kAssumedSequenceSeconds = 1.0f;

    MaterialEditInterface* g_Editor;

    // Nothing here touches the running game unless a panel is open. Every field the panel
    // owns has a value that means something to the panel and nothing to anyone else, and
    // applying those when no one is editing is how the weapon left every player's hands.
    bool Editing()
    {
        return g_Editor != nullptr && g_Editor->IsPanelOpen();
    }

    cl_entity_t g_StandIn;
    std::string g_LoadedPath;

    // The engine keeps every model it has ever loaded in one fixed table, reuses a slot only
    // after a map change has marked it unreferenced, and dies outright when the table is full
    // and nothing is reusable. Nothing here can read how much of it the map has taken, so the
    // editor counts what it has put in and stops far short: an author clicking through a
    // catalogue of three thousand would otherwise reach the end of the table on their own.
    constexpr size_t kMaxModelsOutsidePrecache = 128;

    std::vector<std::string> g_ModelsOutsidePrecache;

    // The studio interface's loader is the engine's own without CRC tracking, which is what
    // keeps a model looked at in the editor out of the server's consistency checks.
    model_t* LoadOutsidePrecache(const char* path)
    {
        if (IEngineStudio.Mod_ForName == nullptr)
        {
            return nullptr;
        }

        bool loaded_before =
            std::find(g_ModelsOutsidePrecache.begin(), g_ModelsOutsidePrecache.end(), path) !=
            g_ModelsOutsidePrecache.end();

        if (!loaded_before && g_ModelsOutsidePrecache.size() >= kMaxModelsOutsidePrecache)
        {
            gEngfuncs.Con_Printf(
                "r_mat_standin: %u models have been loaded for the editor on this map already, "
                "which is as many as it will ask the engine to hold; change map to free them\n",
                static_cast<unsigned>(g_ModelsOutsidePrecache.size())
            );

            return nullptr;
        }

        // Second argument zero: a model that is not there is answered with nothing rather
        // than with a fatal error.
        model_t* model = IEngineStudio.Mod_ForName(path, 0);

        if (model != nullptr && !loaded_before)
        {
            g_ModelsOutsidePrecache.push_back(path);
        }

        return model;
    }

    // One per mover, and not one shared. A single clock is consumed by whoever reads it
    // first, so the second mover always saw no time pass; and the stand-in's copy is only
    // touched while a stand-in exists, which left the hands with a clock that never started.
    float g_StandInTime;
    float g_ViewmodelTime;

    // What the weapon in the author's hands was last published from. The model changes with
    // the weapon, and its animations are not the previous weapon's.
    model_t* g_ViewmodelModel;

    // The model the panel asked to be holding, and what that path resolved to. Remembered
    // because the path is read every frame while the lookup behind it is not free.
    std::string g_ViewmodelWantedPath;
    model_t* g_ViewmodelWanted;

    // A path to a loaded model, by whichever of the two loaders can answer for it. The answer
    // is remembered, including a failure, so a model that is not there is looked for once
    // rather than on every frame the panel is open.
    model_t* ResolveModel(const char* path, std::string& resolved_path, model_t*& resolved)
    {
        if (resolved_path == path)
        {
            return resolved;
        }

        int index = 0;
        model_t* model = gEngfuncs.CL_LoadModel(path, &index);

        if (model == nullptr)
        {
            model = LoadOutsidePrecache(path);
        }

        if (model == nullptr)
        {
            gEngfuncs.Con_Printf("r_mat_editor: cannot load %s\n", path);
        }

        resolved_path = path;
        resolved = model;

        return model;
    }

    // Which animations a model has, by name. Only the client can answer it: the panel is
    // given a path or nothing at all, and the engine is what turns either into a header.
    void PublishSequences(MaterialTarget target, model_t* model)
    {
        int count = 0;
        std::string names;

        if (model != nullptr && IEngineStudio.Mod_Extradata != nullptr)
        {
            const studiohdr_t* header = static_cast<const studiohdr_t*>(IEngineStudio.Mod_Extradata(model));

            if (header != nullptr)
            {
                count = header->numseq;

                const mstudioseqdesc_t* sequences = reinterpret_cast<const mstudioseqdesc_t*>(
                    reinterpret_cast<const byte*>(header) + header->seqindex
                );

                for (int i = 0; i < count; i++)
                {
                    if (i > 0)
                    {
                        names += '\n';
                    }

                    // Numbered as well as named: several models carry the same label on more
                    // than one sequence, and the number is what the panel actually sets.
                    char line[80];
                    std::snprintf(line, sizeof(line), "%d  %s", i, sequences[i].label);
                    names += line;
                }
            }
        }

        g_Editor->SetSequenceNames(target, names.c_str());

        MaterialAnimation animation;
        g_Editor->GetAnimation(target, &animation);

        animation.sequence_count = count;
        animation.sequence = count > 0 ? std::min(animation.sequence, count - 1) : 0;

        g_Editor->SetAnimation(target, &animation);
    }

    // The elapsed time a mover is stepped by, against that mover's own clock. Guarded rather
    // than trusted: a level change or a pause hands back a step that would spin a model
    // through several turns in one frame.
    float Elapsed(float now, float& last)
    {
        float elapsed = last > 0.0f ? now - last : 0.0f;
        last = now;

        return elapsed >= 0.0f && elapsed <= 0.25f ? elapsed : 0.0f;
    }

    // How long one run of a sequence takes, in seconds, as the model itself declares it.
    // Without this the panel plays every animation at one made-up rate, which for a weapon
    // is several times slower than the animation was drawn to be and reads as standing still.
    float SequenceSeconds(model_t* model, int sequence)
    {
        if (model == nullptr || IEngineStudio.Mod_Extradata == nullptr)
        {
            return kAssumedSequenceSeconds;
        }

        const studiohdr_t* header = static_cast<const studiohdr_t*>(IEngineStudio.Mod_Extradata(model));

        if (header == nullptr || sequence < 0 || sequence >= header->numseq)
        {
            return kAssumedSequenceSeconds;
        }

        const mstudioseqdesc_t* description = reinterpret_cast<const mstudioseqdesc_t*>(
            reinterpret_cast<const byte*>(header) + header->seqindex
        ) + sequence;

        if (description->fps <= 0.0f || description->numframes <= 1)
        {
            return kAssumedSequenceSeconds;
        }

        return static_cast<float>(description->numframes - 1) / description->fps;
    }

    void Advance(MaterialAnimation& animation, float elapsed, model_t* model)
    {
        if (!animation.playing)
        {
            return;
        }

        float seconds = SequenceSeconds(model, animation.sequence);

        animation.frame = std::fmod(animation.frame + elapsed / seconds, 1.0f);
    }

    // The frame is held rather than driven by the engine's own clock: stopping on one frame
    // of a reload is the point, and an animtime that keeps moving would walk off it.
    void HoldFrame(cl_entity_t& entity, const MaterialAnimation& animation, float now)
    {
        entity.curstate.sequence = animation.sequence;
        entity.curstate.animtime = now;
        entity.curstate.frame = animation.frame * 255.0f;
        entity.latched.prevanimtime = now;
        entity.latched.prevframe = entity.curstate.frame;
        entity.latched.prevsequence = animation.sequence;
    }

    // Where the author is looking from, which is the camera when it is flying and the player
    // otherwise. False when there is nothing to look from, which is every frame before the
    // local player exists.
    bool Eye(vec3_t out, float& yaw)
    {
        MaterialCamera camera;
        g_Editor->GetCamera(&camera);

        if (camera.active)
        {
            for (int i = 0; i < 3; i++)
            {
                out[i] = camera.origin[i];
            }

            yaw = camera.angles[1];
            return true;
        }

        cl_entity_t* player = gEngfuncs.GetLocalPlayer();
        if (player == nullptr)
        {
            return false;
        }

        // The view angles rather than the entity's: a player model's pitch is not the pitch
        // its owner is looking along.
        vec3_t view;
        gEngfuncs.GetViewAngles(view);

        out[0] = player->origin[0];
        out[1] = player->origin[1];
        out[2] = player->origin[2] + kEyeHeight;
        yaw = view[1];

        return true;
    }

    void Place(MaterialStandIn& state)
    {
        vec3_t eye;
        float yaw = 0.0f;

        if (!Eye(eye, yaw))
        {
            return;
        }

        // Yaw only, so where the stand-in ends up does not depend on how far down the author
        // happened to be looking.
        vec3_t flat;
        flat[0] = 0.0f;
        flat[1] = yaw;
        flat[2] = 0.0f;

        vec3_t forward;
        vec3_t right;
        vec3_t up;
        gEngfuncs.pfnAngleVectors(flat, forward, right, up);

        for (int i = 0; i < 3; i++)
        {
            state.origin[i] = eye[i] + forward[i] * kPlacementDistance;
        }

        state.origin[2] -= kPlacementDrop;

        // Turned to face where the author is looking from, which is the one angle they have
        // already chosen by flying there.
        state.angles[0] = 0.0f;
        state.angles[1] = yaw + 180.0f;
        state.angles[2] = 0.0f;
    }
}

void MaterialPreview_Init(MaterialEditInterface* editor)
{
    g_Editor = editor;
}

int MaterialPreview_ViewportInset()
{
    return Editing() ? g_Editor->GetAppliedInset() : 0;
}

void MaterialPreview_ApplyCamera(ref_params_s* pparams)
{
    if (!Editing())
    {
        return;
    }

    MaterialCamera camera;
    g_Editor->GetCamera(&camera);

    if (!camera.active)
    {
        return;
    }

    if (camera.seed_requested)
    {
        // Started from the finished view rather than from the player: whatever the author
        // was looking at when they asked for the camera should still be in front of it.
        for (int i = 0; i < 3; i++)
        {
            camera.origin[i] = pparams->vieworg[i];
            camera.angles[i] = pparams->viewangles[i];
        }

        camera.seed_requested = false;
        g_Editor->SetCamera(&camera);
    }

    for (int i = 0; i < 3; i++)
    {
        pparams->vieworg[i] = camera.origin[i];
        pparams->viewangles[i] = camera.angles[i];
    }

    // Recomputed rather than left: these were derived from the angles this just replaced,
    // and the engine takes them as the view's own axes.
    gEngfuncs.pfnAngleVectors(pparams->viewangles, pparams->forward, pparams->right, pparams->up);

    // The view model was placed from the view before this ran. Left where it was it would
    // hang in the air over the body, drawn in front of everything as view models are; moved,
    // it stays in the camera's hands, which is the only place it can be judged from.
    cl_entity_t* view_model = gEngfuncs.GetViewModel();

    if (view_model != nullptr)
    {
        for (int i = 0; i < 3; i++)
        {
            view_model->origin[i] = camera.origin[i];
            view_model->curstate.origin[i] = camera.origin[i];
            view_model->angles[i] = camera.angles[i];
            view_model->curstate.angles[i] = camera.angles[i];
        }

        // Except the pitch, which a studio model takes the other way round: this is the same
        // negation the engine applies when it points a view model along a demo's view.
        view_model->angles[0] = -camera.angles[0];
        view_model->curstate.angles[0] = view_model->angles[0];
    }
}

void MaterialPreview_ApplyViewmodel(ref_params_s* pparams)
{
    if (!Editing())
    {
        // Nothing is published while no panel is open, so the next one to open republishes
        // rather than trusting what a closed panel left behind.
        g_ViewmodelModel = nullptr;
        g_ViewmodelWantedPath.clear();
        g_ViewmodelWanted = nullptr;
        return;
    }

    cl_entity_t* view_model = gEngfuncs.GetViewModel();

    if (view_model == nullptr)
    {
        return;
    }

    // A model the panel asked for takes the weapon's place. For this frame only, like the
    // hiding below: the engine puts the weapon's model back before it asks the client for the
    // view again, so nothing here has to be undone.
    const char* wanted = g_Editor->GetViewmodelModel();

    if (wanted != nullptr && *wanted != '\0')
    {
        if (model_t* chosen = ResolveModel(wanted, g_ViewmodelWantedPath, g_ViewmodelWanted))
        {
            view_model->model = chosen;
        }
    }

    // Republished on the frame the model changes, whichever of the two changed it: what is in
    // the author's hands is whatever they last switched to or chose, and its animations are
    // not the previous one's.
    if (view_model->model != g_ViewmodelModel)
    {
        g_ViewmodelModel = view_model->model;
        PublishSequences(MaterialTarget::Viewmodel, g_ViewmodelModel);
    }

    MaterialViewmodel viewmodel;
    g_Editor->GetViewmodel(&viewmodel);

    if (!viewmodel.shown)
    {
        // For this frame only. The engine puts the weapon's model back before it asks the
        // client for the view again, so nothing has to undo this.
        view_model->model = nullptr;

        g_Editor->SetClientReport("hands: hidden by the panel");
        return;
    }

    if (!viewmodel.overriding)
    {
        g_Editor->SetClientReport("hands: left to the game, the panel is not driving them");
        return;
    }

    float now = pparams->time;
    float elapsed = Elapsed(now, g_ViewmodelTime);

    // Advanced here, where the model that gives the animation its length is at hand, and
    // put on the entity by the renderer: the engine overwrites that entity between here and
    // the draw, so writing it here would be writing it for nobody.
    Advance(viewmodel.animation, elapsed, view_model->model);
    g_Editor->SetViewmodel(&viewmodel);

    char report[256];
    std::snprintf(
        report, sizeof(report),
        "hands: %s, sequence %d of %.2fs, frame %.3f, playing %s, step %.4f, time %.2f",
        view_model->model != nullptr ? view_model->model->name : "no model",
        viewmodel.animation.sequence,
        SequenceSeconds(view_model->model, viewmodel.animation.sequence),
        viewmodel.animation.frame,
        viewmodel.animation.playing ? "yes" : "no",
        elapsed,
        now
    );

    g_Editor->SetClientReport(report);
}

void MaterialPreview_VidInit()
{
    // Whatever the editor put into the engine's model table is reusable from here, so the
    // count starts again. The stand-in is dropped with it: its model belongs to the map that
    // just went away.
    g_ModelsOutsidePrecache.clear();
    g_LoadedPath.clear();
    g_ViewmodelWantedPath.clear();
    g_ViewmodelWanted = nullptr;
}

void MaterialPreview_CreateEntities()
{
    if (!Editing())
    {
        return;
    }

    const char* path = g_Editor->GetStandInModel();

    if (path == nullptr || *path == '\0')
    {
        g_LoadedPath.clear();
        return;
    }

    // Loaded through the engine, so it is cached exactly as any other model is and the studio
    // path sees nothing unusual about it.
    if (g_LoadedPath != path)
    {
        // CL_LoadModel loads nothing despite the name: it looks the name up among what the
        // server precached and answers with that index. A model the map has no use for is
        // not in there at all, and has to be loaded here.
        int index = 0;
        model_t* model = gEngfuncs.CL_LoadModel(path, &index);

        if (model == nullptr)
        {
            index = 0;
            model = LoadOutsidePrecache(path);
        }

        if (model == nullptr)
        {
            gEngfuncs.Con_Printf("r_mat_standin: cannot load %s\n", path);
            g_Editor->SetStandInModel("");
            return;
        }

        std::memset(&g_StandIn, 0, sizeof(g_StandIn));

        g_StandIn.model = model;
        g_StandIn.index = index;
        g_StandIn.curstate.modelindex = index;
        g_StandIn.curstate.renderamt = 255;
        g_StandIn.curstate.scale = 1.0f;
        g_StandIn.curstate.framerate = 1.0f;

        g_LoadedPath = path;

        PublishSequences(MaterialTarget::StandIn, model);
    }

    MaterialStandIn state;
    g_Editor->GetStandIn(&state);

    if (g_Editor->TakeStandInPlacement())
    {
        Place(state);
    }

    float now = gEngfuncs.GetClientTime();
    float elapsed = Elapsed(now, g_StandInTime);

    if (state.spinning)
    {
        state.angles[1] = std::fmod(state.angles[1] + kSpinDegreesPerSecond * elapsed, 360.0f);
    }

    Advance(state.animation, elapsed, g_StandIn.model);

    g_Editor->SetStandIn(&state);

    for (int i = 0; i < 3; i++)
    {
        g_StandIn.origin[i] = state.origin[i];
        g_StandIn.curstate.origin[i] = state.origin[i];
        g_StandIn.angles[i] = state.angles[i];
        g_StandIn.curstate.angles[i] = state.angles[i];
    }

    // Zero is "as it was built": the panel has not been opened on this yet, and a model
    // drawn at zero scale is a model nobody can see.
    g_StandIn.curstate.scale = state.scale > 0.0f ? state.scale : 1.0f;

    HoldFrame(g_StandIn, state.animation, now);

    for (int i = 0; i < 3; i++)
    {
        g_StandIn.latched.prevorigin[i] = state.origin[i];
        g_StandIn.latched.prevangles[i] = state.angles[i];
    }

    gEngfuncs.CL_CreateVisibleEntity(ET_NORMAL, &g_StandIn);
}
