#include "main.h"
#include "fov.h"
#include "inspect.h"

#include <cstdint>

#include <goldsrc/engine/model.h>
#include <goldsrc/entity.h>
#include <goldsrc/format/studio.h>
#include <goldsrc/studio_api.h>

namespace
{
    goldsrc::studiohdr_t* GetTextureHeader(goldsrc::model_t* model)
    {
        goldsrc::studiohdr_t* studiohdr = static_cast<goldsrc::studiohdr_t*>(IEngineStudio.Mod_Extradata(model));
        if (studiohdr == nullptr || studiohdr->textureindex != 0)
        {
            return studiohdr;
        }

        // textures are external, stored in <model name>T.mdl
        char name[64 + 2];
        size_t len = strlen(model->name);
        if (len < 4 || len + 2 > sizeof(name))
        {
            return nullptr;
        }

        strcpy(name, model->name);
        strcpy(name + len - 4, "T.mdl");

        goldsrc::model_t* texture_model = IEngineStudio.Mod_ForName(name, false);
        if (texture_model == nullptr)
        {
            return nullptr;
        }

        return static_cast<goldsrc::studiohdr_t*>(IEngineStudio.Mod_Extradata(texture_model));
    }

    // R_GLStudioDrawPoints alpha-tests masked meshes against fixed 0.5 while their fragment alpha is modulated
    // by r_blend, so renderamt < 128 discards every fragment and the mesh disappears; for non-normal rendermodes
    // the mask is dropped for the draw and transparent texels are resolved by blending instead (texel alpha is 0)
    bool ShouldUnmaskTextures(goldsrc::cl_entity_t* entity, int flags)
    {
        return (flags & goldsrc::STUDIO_RENDER) && entity->curstate.rendermode != goldsrc::kRenderNormal && entity->model != nullptr &&
               entity->model->type == goldsrc::mod_studio;
    }

    int UnmaskTextures(goldsrc::model_t* model, goldsrc::mstudiotexture_t** unmasked)
    {
        goldsrc::studiohdr_t* texturehdr = GetTextureHeader(model);
        if (texturehdr == nullptr)
        {
            return 0;
        }

        goldsrc::mstudiotexture_t* textures = reinterpret_cast<goldsrc::mstudiotexture_t*>((uint8_t*)texturehdr + texturehdr->textureindex);

        int count = 0;
        for (int i = 0; i < texturehdr->numtextures && count < goldsrc::MAXSTUDIOSKINS; i++)
        {
            if (textures[i].flags & goldsrc::STUDIO_NF_MASKED)
            {
                textures[i].flags &= ~goldsrc::STUDIO_NF_MASKED;
                unmasked[count++] = &textures[i];
            }
        }

        return count;
    }

    void RemaskTextures(goldsrc::mstudiotexture_t** unmasked, int count)
    {
        for (int i = 0; i < count; i++)
        {
            unmasked[i]->flags |= goldsrc::STUDIO_NF_MASKED;
        }
    }
} // namespace

int StudioDrawModel(int flags)
{
    goldsrc::cl_entity_t* entity = IEngineStudio.GetCurrentEntity();

    if (entity == IEngineStudio.GetViewEntity())
    {
        // StudioSetRenderamt recalculates r_blend via CL_FxBlend, which returns renderamt regardless of rendermode;
        // calling it for goldsrc::kRenderNormal (renderamt 0) zeroes r_blend and alpha test discards masked meshes entirely
        if (entity->curstate.rendermode != goldsrc::kRenderNormal)
        {
            IEngineStudio.StudioSetRenderamt(entity->curstate.renderamt);
        }

        InspectThink();
    }

    if (ShouldUnmaskTextures(entity, flags))
    {
        goldsrc::mstudiotexture_t* unmasked[goldsrc::MAXSTUDIOSKINS];
        int count = UnmaskTextures(entity->model, unmasked);

        int result = g_OriginalStudio.StudioDrawModel(flags);

        RemaskTextures(unmasked, count);
        return result;
    }

    return g_OriginalStudio.StudioDrawModel(flags);
}

int StudioDrawPlayer(int flags, goldsrc::entity_state_t* pplayer)
{
    goldsrc::cl_entity_t* entity = IEngineStudio.GetCurrentEntity();

    if (entity != nullptr && ShouldUnmaskTextures(entity, flags))
    {
        goldsrc::mstudiotexture_t* unmasked[goldsrc::MAXSTUDIOSKINS];
        int count = UnmaskTextures(entity->model, unmasked);

        int result = g_OriginalStudio.StudioDrawPlayer(flags, pplayer);

        RemaskTextures(unmasked, count);
        return result;
    }

    return g_OriginalStudio.StudioDrawPlayer(flags, pplayer);
}
