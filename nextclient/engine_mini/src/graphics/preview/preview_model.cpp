#include "graphics/preview/preview_model.h"

#include "engine.h"

#include <string>

#include <studio.h>
#include <ncl_math/vec3.h>
#include <next_engine_mini/ScenePreviewInterface.h>

#include "common/model.h"
#include "graphics/gl_local.h"
#include "graphics/ViewmodelFrustumCalculator.h"
#include "graphics/preview/preview_view.h"

// The out parameter of the studio api's light query, laid out as com_model.h has it: that
// header cannot stand beside the engine's model.h this file already reaches.
// The tag is the studio api's own: its function pointer is declared over it.
typedef struct alight_s
{
    int ambientlight;
    int shadelight;
    vec3_t color;
    float* plightvec;
} alight_t;

namespace
{
    cl_entity_t g_PreviewEntity;

    int g_AmbientLight;
    int g_ShadeLight;
    float g_LightColor[3];
    float g_LightDirection[3];

    void (*g_EngineDynamicLight)(cl_entity_t* ent, alight_t* plight);
    int (*g_EngineCheckBBox)();

    engine_studio_api_t* g_PatchedApi;

    model_t* g_Model;
    std::string g_ModelPath;

    void PreviewDynamicLight(cl_entity_t* ent, alight_t* plight)
    {
        if (ent != &g_PreviewEntity)
        {
            g_EngineDynamicLight(ent, plight);
            return;
        }

        plight->ambientlight = g_AmbientLight;
        plight->shadelight = g_ShadeLight;

        ncl_math::VectorCopy(g_LightColor, plight->color);
        ncl_math::VectorCopy(g_LightDirection, plight->plightvec);
    }

    // The engine tests against the frustum of the last frame it drew itself, which after a
    // map is the player's view; a preview is drawn through a view of its own.
    int PreviewCheckBBox()
    {
        if (*p_currententity == &g_PreviewEntity)
        {
            return TRUE;
        }

        return g_EngineCheckBBox();
    }

    mstudioseqdesc_t* GetSequence(studiohdr_t* header, int sequence)
    {
        if (sequence < 0 || sequence >= header->numseq)
        {
            return nullptr;
        }

        return reinterpret_cast<mstudioseqdesc_t*>(reinterpret_cast<byte*>(header) + header->seqindex) + sequence;
    }

    void SetupEntity(model_t* model, int sequence, float frame, float time, const float* origin, const float* angles)
    {
        // Never the view entity's index: with cl_righthand on, the engine takes an entity
        // holding it for its own flipped view model and turns culling and lighting over.
        g_PreviewEntity.index = -1;
        g_PreviewEntity.player = 0;
        g_PreviewEntity.model = model;

        entity_state_t& state = g_PreviewEntity.curstate;

        ncl_math::VectorCopy(origin, g_PreviewEntity.origin);
        ncl_math::VectorCopy(angles, g_PreviewEntity.angles);
        ncl_math::VectorCopy(origin, state.origin);
        ncl_math::VectorCopy(angles, state.angles);

        state.movetype = MOVETYPE_NONE;
        state.sequence = sequence;

        // the engine advances the frame from animtime by framerate, so zero holds the pose
        state.frame = frame;
        state.animtime = time;
        state.framerate = 0.0f;

        state.scale = 1.0f;
        state.rendermode = kRenderNormal;
        state.renderamt = 255;
        state.body = 0;
        state.skin = 0;

        for (int i = 0; i < 4; i++)
        {
            state.controller[i] = 127;
        }

        state.blending[0] = 0;
        state.blending[1] = 0;

        // Latched to the pose being drawn: the renderer blends from these, and a value left
        // behind throws the next frames across the map.
        g_PreviewEntity.latched.prevanimtime = state.animtime;
        g_PreviewEntity.latched.prevsequence = state.sequence;
        g_PreviewEntity.latched.prevframe = state.frame;
        g_PreviewEntity.latched.sequencetime = state.animtime;

        ncl_math::VectorCopy(origin, g_PreviewEntity.latched.prevorigin);
        ncl_math::VectorCopy(angles, g_PreviewEntity.latched.prevangles);
    }
} // namespace

void PreviewModel_PatchStudioApi(engine_studio_api_t* api)
{
    // The table is the engine's own global and outlives one query of it, so a second patch
    // would take the wrappers below for the engine's functions and recurse for ever.
    if (api->StudioDynamicLight == &PreviewDynamicLight)
    {
        return;
    }

    g_PatchedApi = api;

    g_EngineDynamicLight = api->StudioDynamicLight;
    api->StudioDynamicLight = &PreviewDynamicLight;

    g_EngineCheckBBox = api->StudioCheckBBox;
    api->StudioCheckBBox = &PreviewCheckBBox;
}

void PreviewModel_RestoreStudioApi()
{
    if (g_PatchedApi == nullptr)
    {
        return;
    }

    g_PatchedApi->StudioDynamicLight = g_EngineDynamicLight;
    g_PatchedApi->StudioCheckBBox = g_EngineCheckBBox;

    g_PatchedApi = nullptr;
    g_EngineDynamicLight = nullptr;
    g_EngineCheckBBox = nullptr;
}

int PreviewModel_Draw(const PreviewModel& model, const PreviewLight& light, const PreviewCamera& camera, int wide, int tall)
{
    if (pStudioAPI == nullptr || pStudioAPI->StudioDrawModel == nullptr)
    {
        return -1;
    }

    if (wide <= 0 || tall <= 0 || model.path == nullptr || model.path[0] == '\0')
    {
        return -1;
    }

    // Once per path, a missing file included. Tracked: the entry a later precache of the same
    // file finds has to carry a consistency baseline, which an untracked first read leaves out.
    if (g_ModelPath != model.path)
    {
        g_Model = Mod_ForName(model.path, false, true);
        g_ModelPath = model.path;
    }

    if (g_Model == nullptr || g_Model->type != mod_studio)
    {
        return -1;
    }

    // every draw: the cache can drop the model's data, and Mod_Extradata loads it again
    studiohdr_t* header = static_cast<studiohdr_t*>(Mod_Extradata(g_Model));
    if (header == nullptr)
    {
        return -1;
    }

    if (GetSequence(header, model.sequence) == nullptr)
    {
        return -1;
    }

    bool mirror = model.camera_space && model.mirror;

    if (model.camera_space)
    {
        PreviewView_LoadCameraSpace(ViewmodelFrustumCalculator::CalcVerticalFov(model.fov), mirror, wide, tall);
    }
    else
    {
        PreviewView_LoadCamera(camera, wide, tall);
    }

    // The mirror turns the space over about the lateral axis: the model is placed on the
    // other side of it to stay put, and the angles about the mirrored axes, and the light,
    // change hand with it.
    float origin[3] = {model.origin[0], mirror ? -model.origin[1] : model.origin[1], model.origin[2]};
    float angles[3] = {model.angles[0], mirror ? -model.angles[1] : model.angles[1], mirror ? -model.angles[2] : model.angles[2]};

    g_AmbientLight = light.model_ambient;
    g_ShadeLight = light.model_shade;

    ncl_math::VectorCopy(light.color, g_LightColor);
    ncl_math::VectorCopy(light.direction, g_LightDirection);

    if (mirror)
    {
        g_LightDirection[1] = -g_LightDirection[1];
    }

    SetupEntity(g_Model, model.sequence, model.frame * 256.0f, gEngfuncs.GetClientTime(), origin, angles);

    // The engine culls front faces and turns the test back on after every body part, so a
    // mirrored model is drawn with the winding turned over rather than without the test.
    qglEnable(GL_CULL_FACE);
    qglCullFace(GL_FRONT);
    qglFrontFace(mirror ? GL_CW : GL_CCW);

    // Vertex alpha comes from this, and the entity pass that sets it never runs in the menu:
    // it holds whatever the last map left, zero on a fresh start.
    float saved_blend = *p_r_blend;
    *p_r_blend = 1.0f;

    cl_entity_t* saved_current = *p_currententity;
    *p_currententity = &g_PreviewEntity;

    int drawn = pStudioAPI->StudioDrawModel(STUDIO_RENDER);

    *p_currententity = saved_current;
    *p_r_blend = saved_blend;
    qglFrontFace(GL_CCW);

    return drawn;
}
