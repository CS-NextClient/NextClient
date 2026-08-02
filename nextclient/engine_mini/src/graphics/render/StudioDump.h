#pragma once

#include <cstdint>

#include <ncl_math/vec3.h>

namespace render
{
    // What the submission stage was working with for one StudioDrawPoints call, which is
    // one body part of one model. The pass parameters are part of the record because the
    // questions this answers are about them: which model the header ended up paired with,
    // whether the forced chrome pass was taken, what colour the shell was given, whether a
    // material was found and where it came from.
    struct StudioSubmission
    {
        int entity_index;
        bool view_entity;
        uint32_t submodel_offset;
        int forced_face_flags;
        int renderfx;
        int rendermode;
        int effective_rendermode;
        float blend;
        bool glow_shell;
        ncl_math::Vector3 shell_color;
        float glow_scale;
        int meshes;
        uint32_t shade_modes;   // one bit per mode queued, since a body part can mix them
        int materials;
        int server_bound;
        int entity_lights;
    };

    struct StudioOutcome
    {
        const char* model;
        const char* header;
        const char* reason;
        bool drawn;
    };

    // False for every frame except the one r_studio_dump asked for, which is why the
    // submission reads it once and then skips filling the record at all: the record serves
    // one frame of a session, and the submission runs hundreds of times per frame.
    bool Dump_Armed();

    void Dump_Note(const StudioSubmission& submission, const StudioOutcome& outcome);

    // Writes the frame and disarms. Called where the scene ends, so that the view model,
    // which is drawn after everything else, is in the same frame as the rest.
    void Dump_OnSceneEnd();

    void Dump_RegisterCommands();
}
