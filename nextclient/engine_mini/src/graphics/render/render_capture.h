#pragma once

namespace render
{
    void Capture_RegisterCommands();

    // Captures the completed scene before HUD counters and clocks are drawn.
    void Capture_OnSceneEnd();

    // Restores saved cvars while their engine-owned storage is still valid.
    void Capture_Cancel();
} // namespace render
