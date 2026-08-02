#pragma once

// Image-diff regression foundation. On the "radar_shot [file]" console command, the
// next fully-composited HUD frame (the blurred radar backdrop, flashbang wash, smoke,
// markers and the HUD text pushed out from under the radar) is read back from the
// framebuffer and written as an uncompressed TGA. A deterministic scenario can then be
// regression-checked by a tolerant pixel diff against a committed golden, covering the
// visual aspects the marker-model dump cannot (see RadarDump). The diff is run on a
// fixed machine/resolution: read-back pixels depend on the GPU, driver and video mode.
namespace RadarShot
{
    void Init(); // register the radar_shot console command (call once at startup)

    // Arm a capture of the next composited frame to the given file (game-dir relative).
    // Used by the test plan scheduler; equivalent to the radar_shot command.
    void Request(const char* file);

    // Reads back and writes the frame when a shot was requested. Call at the very end of
    // the HUD frame (after the stock HUD text and NextClient's own HUD have drawn) so the
    // capture includes everything. Cheap no-op when no shot is pending.
    void CaptureIfPending();
} // namespace RadarShot
