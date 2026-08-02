#pragma once

// Radar regression-test foundation. On the "radar_dump [file]" console command, the
// advanced radar serializes the marker model it computes on the next frame to a JSON
// file. A deterministic scenario (demo playback) can then be regression-checked by
// diffing the dump against a committed golden, instead of running a manual checklist.
namespace RadarDump
{
    void Init(); // register the radar_dump console command (call once at startup)

    // Arm a dump of the next radar frame to the given file (game-dir relative). Used by
    // the test plan scheduler; equivalent to the radar_dump command.
    void Request(const char* file);

    // True while the radar should record markers this frame (set by BeginFrame only on
    // a frame a dump was requested for, cleared by EndFrame).
    bool Capturing();

    void BeginFrame(float time, float box_x, float box_y, float box_size);

    // World position (resolution-independent input), the resulting on-screen position,
    // colour and alpha of one marker. Cheap no-op unless Capturing().
    void AddMarker(const char* kind, int id, float wx, float wy, float sx, float sy, int r, int g, int b, int a, const char* flags);

    void EndFrame(); // writes the JSON and clears the pending flag
} // namespace RadarDump
