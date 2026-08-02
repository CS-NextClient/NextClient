#pragma once

namespace render
{
    // Diagnostics overlay for the studio stage (r_studio_graph), modelled on the radar's
    // hud_radar_debug panel: rolling history of the frame time and of the time spent
    // inside the submission stage, so toggling r_studio_shader shows its cost live
    // instead of through a separate benchmark run.
    void Overlay_Draw();
}
