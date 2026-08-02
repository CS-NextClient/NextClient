#pragma once

// studio.h uses vec3_t without declaring it. The studio types are typedefs of anonymous
// structs, so they cannot be forward declared and the header has to be pulled in.
#include <tier0/basetypes.h>
#include <studio.h>

typedef struct model_s model_t;

namespace render
{
    // Draws one submodel through the shader path. Returns false whenever anything is
    // missing or not handled yet, and the caller then falls back to the engine's own
    // submission, so the stage is always switchable and never half-drawn.
    // submodel is whatever StudioSetupModel wrote into its out parameter, passed on
    // untouched: the engine hands out the address of its own submodel pointer rather
    // than the pointer itself, and which of the two it is has to be decided against the
    // model buffer rather than assumed.
    bool Studio_TryDraw(model_t* model, const studiohdr_t* header, void* submodel);

    // Drops whatever is still queued. The queue normally empties itself once a model's
    // last body part has been submitted; one that survives to the end of the scene cannot
    // be drawn faithfully any more (the skin bind reads the entity as it stands), so the
    // safety net discards it rather than carry it into the next frame.
    void Studio_DiscardPending();

    // Why the last attempt fell back to the engine's own submission. Reported by
    // r_studio_info, since every fallback is otherwise silent by design.
    const char* Studio_LastRejection();

    // Builds every program a model can be drawn with, at start-up instead of at the first
    // draw that needs one. The set is the built-in program plus every shipped source pair
    // that carries a manifest, which is what makes a program reachable by a material.
    void Studio_WarmPrograms();

    // The studio program carries a copy of lightgammatable, so it has to be re-uploaded
    // whenever the engine rebuilds it.
    void Studio_OnGammaChanged();
}
