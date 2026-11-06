#pragma once

#include <cstdint>

#include <goldsrc/fwd.h>
#include <goldsrc/studio_api.h>

namespace performance
{
    class PerformanceMonitor;
}

namespace render
{
    // Patches the engine studio API table before client.dll copies it into its own
    // renderer. This is the only way to reach the geometry submission stage without a
    // new static hook into hw.dll: the client resolves StudioDrawPoints once, from the
    // table it is handed, so a post-hook is already too late.
    // The lighting inputs the client hands the engine for the entity being drawn. Copied
    // rather than referenced: plightvec points into the client's own frame local.
    struct StudioLighting
    {
        float ambient;
        float shade;
        float color[3];
        float light_vec[3];
        bool valid;
    };

    class StudioRenderer;
    struct StudioEngineBindings;
    struct StudioModelInputs;

    struct StudioInterceptCallbacks
    {
        goldsrc::engine_studio_api_t studio{};
        void (*triangle_begin)(int){};
    };

    class StudioInterceptState
    {
    private:
        performance::PerformanceMonitor& monitor_;
        unsigned int& studio_depth_;
        StudioRenderer& renderer_;
        StudioEngineBindings& engine_;
        goldsrc::engine_studio_api_t original_{};
        StudioInterceptCallbacks installed_;
        goldsrc::engine_studio_api_t* patched_{};
        goldsrc::triangleapi_t* patched_triangles_{};
        void (*original_triangle_begin_)(int){};
        bool pass_through_{};
        goldsrc::studiohdr_t* header_{};
        goldsrc::model_t* model_{};
        void* submodel_{};
        uint32_t draw_points_calls_{};
        uint32_t shader_draw_calls_{};
        uint32_t model_draw_serial_ = 1;
        goldsrc::model_t* extradata_model_{};
        void* extradata_result_{};
        StudioLighting lighting_{};
        bool shader_drew_last_{};

    public:
        StudioInterceptState(
            StudioRenderer& renderer,
            StudioEngineBindings& engine,
            performance::PerformanceMonitor& monitor,
            unsigned int& studio_depth
        );
        StudioInterceptState(const StudioInterceptState&) = delete;
        StudioInterceptState& operator=(const StudioInterceptState&) = delete;
        void PatchApi(goldsrc::engine_studio_api_t* api, goldsrc::triangleapi_t* triangles, const StudioInterceptCallbacks& callbacks);
        const StudioLighting& get_lighting() const;
        uint32_t get_model_draw_serial() const;
        const goldsrc::engine_studio_api_t* get_original_api() const;
        void Shutdown();
        void ForgetModel();
        void ForgetModel(const goldsrc::model_t* model);
        StudioModelInputs CaptureInputs(goldsrc::model_t* model, const goldsrc::studiohdr_t* header, void* submodel) const;
        void* ModExtradata(goldsrc::model_t* model);
        void StudioSetHeader(void* header);
        void SetRenderModel(goldsrc::model_t* model);
        void StudioSetupLighting(goldsrc::alight_t* plight);
        void StudioSetupModel(int bodypart, void** ppbodypart, void** ppsubmodel);
        void StudioDrawPoints();
        void GL_StudioDrawShadow();
        void RestoreRenderer();
        void StudioDrawBones();
        void StudioDrawHulls();
        void StudioDrawAbsBBox();
        void StudioRenderShadow(int count, float* p1, float* p2, float* p3, float* p4);
        void TriangleBegin(int primitive);
        void StudioInfo_f();
    };

    const StudioLighting& Studio_Lighting();

    // Changes on header adoption and current model invalidation. A draw must recheck it
    // after model-loading callbacks before consuming borrowed inputs.
    uint32_t Studio_ModelDrawSerial();

    void Studio_PatchApi(goldsrc::engine_studio_api_t* api);

    // The unpatched studio API table retains access to engine-owned state.
    const goldsrc::engine_studio_api_t* Studio_OriginalApi();

    // Puts the engine's table back and, more importantly, makes the entry points we
    // handed out pass straight through. The client holds a copy of the table, so the
    // pointers cannot be recalled; they can only be made harmless.
    void Studio_Shutdown();

    // Drops the model, header and submodel the last submission left behind. They point
    // into the engine's model cache, which a map change reuses, and r_studio_info parses
    // the header as a live file.
    void Studio_ForgetModel();
    // Requires a drained or discarded queue; preserves unrelated active inputs.
    void Studio_ForgetModel(const goldsrc::model_t* model);

    void Studio_RegisterCommands();
} // namespace render
