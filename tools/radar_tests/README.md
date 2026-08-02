# Radar regression checks

Two complementary captures replace manual per-release inspection of the advanced
radar. Both are console commands in `client_mini`, both write into the game
directory (the `cstrike` folder) and print the absolute path they wrote.

| Command | Output | Covers |
| --- | --- | --- |
| `radar_dump [file.json]` | marker model (kind, world/screen xy, colour, flags) | marker placement, team colours, objective letters, bomb/hostage logic |
| `radar_shot [file.tga]` | full composited HUD frame (TGA) | blurred backdrop, flashbang wash, smoke, text pushed out from under the radar, multi-resolution layout |

Both capture the **next** drawn frame, so invoke them while in a game with the
advanced radar actually on screen (a live round, radar visible). From the main
menu, the classic radar, or with the HUD hidden nothing is written.

## Image-diff workflow (`radar_shot`)

The frame is read back from the GPU framebuffer, so the pixels depend on the
GPU, the driver and the video mode. The check is therefore **machine- and
resolution-bound**: run it on the same machine and resolution the golden was
captured on, and recapture goldens after a hardware/driver change or an
intentional radar change. Comparison is tolerant (anti-aliasing and the live
blur are never bit-exact), so it flags real visual regressions, not sampling
noise.

1. Reach a deterministic scene (a recorded demo paused at a fixed tick is ideal:
   `playdemo <demo>`, then `pause` at the moment of interest, so game time and
   thus the flash/smoke/blur state are frozen).
2. Capture the golden once and commit it:
   `radar_shot golden_<scenario>_<WxH>.tga`
3. On each release, reproduce the same scene at the same resolution and capture:
   `radar_shot check_<scenario>_<WxH>.tga`
4. Diff:
   `python tools/radar_tests/imgdiff.py golden_<...>.tga check_<...>.tga --out diff.png`

`imgdiff.py` exits 0 within tolerance, 1 on a real difference, and writes a
heatmap (`--out`) showing where pixels changed. Use `--region X,Y,W,H` to
restrict the comparison to the radar box, `--tol` / `--fail-frac` to adjust
sensitivity. Requires `pillow` and `numpy`.

`--clusters` adds a second verdict for the case the fraction cannot decide: it
groups the differing pixels by the tiles they fill densely and fails on a group
that is both dense and one-sided, whatever the fraction says. A defect of that
shape passed a fraction limit once, on the studio shader path.

For multi-resolution coverage, capture a golden per target resolution
(`..._1920x1080.tga`, `..._1280x1024.tga`, ...) and diff each against a check
captured at the matching resolution.

## Marker-model workflow (`radar_dump`)

`radar_dump` is GPU-independent (it serialises computed coordinates), so its
goldens are portable and can run anywhere. Diff with any JSON-aware tool, e.g.
`git diff --no-index golden.json check.json`.

## Unattended run (`radar_harness.py`)

`radar_harness.py` drives a demo end to end and diffs everything in one command,
so a release check is a single invocation instead of manual console input.

How it works: the client schedules captures itself. The harness writes a plan
file the client auto-loads, launches the demo, waits for the client's
done-marker, then collects and diffs the outputs.

- `RadarTestPlan` (in the client) auto-loads `<gamedir>/radar_testplan.txt` at
  startup and consumes (deletes) it, so the harness only has to drop that file
  and launch a demo. Each step fires its captures when its trigger is satisfied,
  then a `radar_testplan.done` marker is written. The `radar_testplan <file>`
  console command loads a plan manually.

Triggers (no manual timeline markup is required: a step can react to a game
event, sample periodically, or fire at a fixed time). Plan line forms:

- `on <flash|smoke|bomb|hostage> [delay] <dump|shot|both> <base>` - fires when the
  event first occurs, `delay` seconds later (sensible per-event default: flash
  0.10, smoke 1.20, bomb 0.30). This is the recommended form: record a demo that
  contains the events and the captures land themselves.
- `every <seconds> <count> <dump|shot|both> <base>` - samples the demo `count`
  times at that interval (files `<base>_0`, `<base>_1`, ...). A catch-all that
  needs no knowledge of the demo content.
- `at <seconds> <dump|shot|both> <base>` (or the bare `<seconds> ...`) - fixed
  elapsed demo time (`GetClientTime`, reproducible). Useful for the opening frame.

Directives `quit_after_last <0|1>` and `done <marker>`; comments start with `#`.

Steps:

1. Record a deterministic demo (`record radar_test` in a scripted/bot scenario)
   containing the moments to check (a smoke, a flash, a bomb plant) and place it
   in `cstrike`. With event triggers there is nothing to time by hand.
2. Copy `sample_config.json`, set `exe`/`gamedir`/`demo`/`resolution` and the
   `steps`. Each step has a `trigger` (`on` with `event`/optional `delay`,
   `every` with `interval`/`count`, or `at` with `time`), a `name`, `dump`/`shot`
   and an optional `region` for image steps.
3. Establish goldens once on this machine/resolution:
   `python radar_harness.py myconfig.json --update`
4. On each release, run the check:
   `python radar_harness.py myconfig.json`
   Exit 0 = all steps within tolerance; non-zero = a regression or a missing
   capture (a `*_diff.png` heatmap is left in the game dir for failing image
   steps unless `--keep` is off).

`+host_framerate` in the launch args fixes the demo timestep, which both speeds
the run up and makes the captured frame reproducible run to run. Multi-resolution
coverage: run the harness once per resolution (each writes resolution-suffixed
files, e.g. `smoke_1280x1024.tga`), with a golden set per resolution.

Note: `radar_testplan.txt` in `cstrike` auto-runs on the next launch that plays a
demo. The harness removes it after each run; remove it by hand if a run was
interrupted.
