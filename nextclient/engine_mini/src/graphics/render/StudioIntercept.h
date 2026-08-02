#pragma once

struct engine_studio_api_s;

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

    const StudioLighting& Studio_Lighting();

    struct StudioFrameStats
    {
        float submission_ms;
        uint32_t calls;
        uint32_t shader_calls;
    };

    // Returns what accumulated since the last call and starts a new frame. Timing the
    // hook covers both paths, so the original submission is measured by the same
    // instrument as the replacement.
    StudioFrameStats Studio_TakeFrameStats();

    // Incremented every time the engine is handed a new studio header, which happens once
    // per model draw, before its body part loop. Work that is constant across the body
    // parts of one model keys off this instead of repeating per submodel.
    uint32_t Studio_ModelDrawSerial();

    void Studio_PatchApi(engine_studio_api_s* api);

    // The table as it was before patching. The draw path calls back into it for the
    // state the engine still owns, such as the bone transforms.
    const engine_studio_api_s* Studio_OriginalApi();

    // Puts the engine's table back and, more importantly, makes the entry points we
    // handed out pass straight through. The client holds a copy of the table, so the
    // pointers cannot be recalled; they can only be made harmless.
    void Studio_Shutdown();

    // Drops the model, header and submodel the last submission left behind. They point
    // into the engine's model cache, which a map change reuses, and r_studio_info parses
    // the header as a live file.
    void Studio_ForgetModel();

    void Studio_RegisterCommands();
}
