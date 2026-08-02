#pragma once

namespace render
{
    void Capture_RegisterCommands();

    // Called at the end of the scene, before the HUD is drawn. Keeping the HUD out of
    // the frame keeps clocks, ammo counters and the like out of the comparison.
    void Capture_OnSceneEnd();
}
