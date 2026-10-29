#pragma once

#include <goldsrc/format/studio.h>
#include <goldsrc/fwd.h>

#include "graphics/render/StudioQueue.h"

namespace render
{
    class StudioRenderer;

    class StudioPlayerEffectsScope
    {
    private:
        StudioRenderer& renderer_;
        const goldsrc::cl_entity_t* previous_entity_{};
        bool previous_effects_{};
        uint64_t cancellation_epoch_{};

    public:
        // May load attachment MDLs, so the parent header and submodel must not be acquired yet.
        explicit StudioPlayerEffectsScope(const goldsrc::cl_entity_t* entity);
        StudioPlayerEffectsScope(StudioRenderer& renderer, const goldsrc::engine_studio_api_t* api, const goldsrc::cl_entity_t* entity);
        ~StudioPlayerEffectsScope();

        StudioPlayerEffectsScope(const StudioPlayerEffectsScope&) = delete;
        StudioPlayerEffectsScope& operator=(const StudioPlayerEffectsScope&) = delete;
    };

    class StudioQueuePause
    {
    private:
        StudioRenderer& renderer_;
        bool deferred_{};
        uint64_t cancellation_epoch_{};

    public:
        StudioQueuePause();
        explicit StudioQueuePause(StudioRenderer& renderer);
        ~StudioQueuePause();

        StudioQueuePause(const StudioQueuePause&) = delete;
        StudioQueuePause& operator=(const StudioQueuePause&) = delete;
    };

    // Accepts one submodel. The engine's indirect submodel output is resolved against
    // the captured header; rejected submissions retain no geometry in the queue.
    bool Studio_TryDraw(goldsrc::model_t* model, const goldsrc::studiohdr_t* header, void* submodel);

    void Studio_BindEngine();
    void Studio_BeginOpaquePass();
    void Studio_EndOpaquePass();
    void Studio_EndModel();
    void Studio_Flush(StudioFlushReason reason);
    const StudioQueueStats& Studio_QueueStats();

    // Discards queued work without accessing GL or engine-owned resources.
    void Studio_DiscardPending();
    void Studio_ReleaseContextResources();
    void Studio_InvalidateContextReferences();
    // Requires a drained or discarded queue before retiring one engine model owner.
    void Studio_ForgetDrawModel(const goldsrc::model_t* model);

    // The reason the last submission fell back to the engine renderer.
    const char* Studio_LastRejection();

    const char* Studio_ShaderDefines();

    // Builds every program a model can be drawn with, at start-up instead of at the first
    // draw that needs one. The set is the built-in program plus every shipped source pair
    // that carries a manifest, which is what makes a program reachable by a material.
    void Studio_WarmPrograms();

    // Copies the initialized engine gamma table into the material runtime.
    void Studio_OnGammaChanged();
} // namespace render
