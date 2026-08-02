#pragma once

// Demo-driven capture scheduler for unattended radar regression runs. A plan file lists
// capture steps keyed by elapsed demo time; while a demo plays back, each step fires the
// radar_dump / radar_shot captures at its time, then a done-marker file is written so an
// external harness (tools/radar_tests/radar_harness.py) knows the run finished and can
// collect and diff the outputs against committed goldens.
//
// At startup a plan at "<gamedir>/radar_testplan.txt" is auto-loaded and consumed
// (deleted), so the harness only has to drop that file and launch the demo; the
// radar_testplan console command loads an arbitrary plan for manual use.
namespace RadarTestPlan
{
    void Init(); // register the command and auto-load a pending plan (call once at startup)

    // Advance the schedule. Call once at the very end of the HUD frame, after
    // RadarShot::CaptureIfPending, so a step's dump and shot land on the same next frame.
    void Tick();
} // namespace RadarTestPlan
