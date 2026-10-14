# Radar layers verification procedure

[Russian version](radar-layers.ru.md) | [Reference](../../assets/cstrike/overviews/radar_layers.md) | [Acceptance cases](radar-layer-cases.md)

## Automated checks

Use an x86 development environment and repository dependencies. Reconfigure after source-list changes; specify one target per build. Windows example with the tracked Visual Studio 2022 test preset:

```powershell
cmake --preset vs2022-tests
cmake --build --preset vs2022-tests-release --target client_mini
cmake --build --preset vs2022-tests-release --target client_mini-tests
cmake --build --preset vs2022-tests-release --target view-tests
cmake --build --preset vs2022-tests-release --target crosshair-tests
cmake --build --preset vs2022-tests-release --target gameui
& './build/vs2022-tests/out/bin/Release/client_mini-tests.exe'
& './build/vs2022-tests/out/bin/Release/view-tests.exe'
& './build/vs2022-tests/out/bin/Release/crosshair-tests.exe'
```

Build `gameui` when shared client headers are affected. Local Ninja Multi-Config test presets require `NEXTCLIENT_BUILD_TESTS=ON`, an imported MSVC x86 environment, and a named build configuration. Record the exact compiler, preset, configuration, source commit, commands, exit codes, and logs.

The separate `radar-gl-tests` target renders the production `radar.vert` and `radar_mask.frag` shaders into a 128-pixel RGBA framebuffer. It checks alpha coverage, premultiplied edge color, flash fade, round/square clipping, and unchanged opaque details across 12 scale/rotation combinations. Shader files are read from the configured source checkout at run time. Windows uses a hidden WGL compatibility context; Linux uses a surfaceless EGL pbuffer and requires a 32-bit `libEGL.so.1` with desktop OpenGL support. Linux CTest execution selects software rendering with `LIBGL_ALWAYS_SOFTWARE=1`. These tests require OpenGL 3.0 or newer and fail if context creation, shader compilation, linking, or rendering is unavailable. They do not launch the game.

The same target also calls the production primitive batch renderer with embedded shaders. Bombs at other-floor opacity and player crossfades must retain identical pixels when an inherited alpha test is enabled with different cutoffs. The alpha-test enable flag, function, and reference must be restored after drawing. These checks use the test GL context without loading the game engine. Player marker pairs also cover coincident discs and edge arrows, partial shape overlap, other-layer opacity, separate fixed positions, inherited alpha tests, and 32 simultaneous fades over repeated frames. The shared pixels must retain the weighted coverage of both marker images.

The GL target also checks a map without entry points and with a collision-blocked roof. Flat and Textured ground pixels must match the same ground rendered without that roof. Contact tests cover disconnected free regions, open ground, raised sloped contact, missing collision hulls, contact below the player center, and cancellation with and without entry points.

The GL target also renders production layer composites in flat and textured modes. Pixel checks cover active-layer priority, nearest context selection, lower-floor dimming, unchanged upper-floor colors, transparent regions without geometry, and hidden active meshes. Height-indicator checks compare rendered player and edge-arrow pixels at 24 angles in both height directions; placement tests retain the full marker clamp extent when image height is smaller.

```powershell
cmake --build --preset vs2022-tests-release --target radar-gl-tests
ctest --test-dir build/vs2022-tests -C Release -L gl --output-on-failure
```

On Linux, build the same target in the configured x86 test directory, then run `ctest --test-dir <build> -C Release -L gl --output-on-failure`. The GL tests are separate from the engine-free `client_mini-tests` executable and must be included when verifying shader changes.

Run `RadarLayerScheduledJobTest.DestructionJoinsWithoutPumpingFrames` through CTest to enforce its 10-second shutdown timeout.

Real-map acceptance requires a non-skipped case:

```powershell
$env:NEXTCLIENT_RADAR_BSP = '<absolute path to scp_foundation_bw40.bsp>'
& './build/vs2022-tests/out/bin/Release/client_mini-tests.exe' '--gtest_filter=RadarBspFile.LocalMapAnalysis'
```

An unset/empty variable explicitly skips the case and does not satisfy acceptance. The test-only BSP30 loader validates bounded reads/references and supplies owned polygons through the production adapter. Deterministic fixture UVs verify geometry, not live materials/lightmaps. Keep map data outside Git. Set `NEXTCLIENT_RADAR_MESH_EXPORT` to an output JSON path to export original polygons and per-layer meshes for offline coverage comparisons; the parent directory is created automatically. The export preserves geometry and both UV sets, without textures or live GL state.

Set `NEXTCLIENT_RADAR_CORRECTIONS` to a correction file to apply it before both deterministic builds. The offline reader accepts whitespace-separated tokens and quoted values; use the shown format without comments. For `cs_assault`, use `assets/cstrike/overviews/cs_assault_layers.txt` and `NEXTCLIENT_RADAR_EXPECTED_LAYERS=1`. Compare the exported warehouse interior and retained exterior coverage. Unset correction variables before testing other maps.

Retain group bounds/heights/areas/reasons, input/mesh counts, analysis duration, memory lower bound, and Below/Nearest/Intersect/volume-style timings. The case verifies finite bounds, positive areas, nonempty meshes, independent stacked supports when multiple layers are present, and exact repeated-build mesh/sample equality. Run it for de_dust2, adv_clocktower, adv_university, and scp_foundation_bw40. Record `capture_ms` separately from worker `analysis_ms` in runtime dumps. During analysis, exercise a map/video reset, switching between modes 5 and 6, leaving layered modes, and client shutdown. de_dust2 should produce one automatic layer; the other maps must retain their substantial stacked areas. Record overlap_area_threshold, largest_overlap_area, and significant_overlap_regions to explain the grouping. Compare performance in the same configuration, with Release results recorded separately. Memory lower bounds exclude index/container overhead; process peaks include two simultaneous map owners in this test.

Linux x86 checks with native dependencies configured:

```bash
cmake --preset linux -D NEXTCLIENT_BUILD_TESTS=ON
cmake --build --preset linux-debug --target client_mini
cmake --build --preset linux-debug --target client_mini-tests
cmake --build --preset linux-debug --target view-tests
cmake --build --preset linux-debug --target crosshair-tests
cmake --build --preset linux-debug --target gameui
export NEXTCLIENT_RADAR_BSP='<absolute Linux path to scp_foundation_bw40.bsp>'
./build/linux/out/bin/Debug/client_mini-tests
./build/linux/out/bin/Debug/view-tests
./build/linux/out/bin/Debug/crosshair-tests
```

Record an unavailable configure/build check with its actual failing dependency. Compiler availability alone is insufficient. Finish with `git diff --check <implementation-base>`.

## Runtime evidence

Builds write to the output directory; the game loads a separate installation. Identify the loaded client binary and record its commit/configuration. The developer starts the game and executes the linked cases in modes 5 and 6, including the expanded radar. Automated checks do not install the build or launch the game.

For each runtime case, record map/route, map mode, observer mode, expected support/floor, client time, screenshot/video, and `radar_layers_dump` before/during/after. Use `pass`, `fail`, or `not_run`. Failures include the exact trigger and active/candidate/source/reason/cache fields. Repeated dumps read state without geometry or GL generation.

For simultaneous player-marker flicker, enable `hud_radar_debug 1` before reproducing it and run `radar_markers_dump` immediately afterward. During live play, the command retains 128 history samples at intervals of at least 50 ms, including shorter losses and fades between samples. Mask text is formatted inside the client and sent to the engine console as a complete string. Each hexadecimal mask uses bit 0 for player 1. `drawn`, `snapshot`, and `broadcast` describe the selected sources; `updated`, `fading`, and `lost` retain report updates, fades, and disappeared jobs within the sample. `fade_in` identifies appearance without an old copy, `overlap` identifies crossfade copies closer than one projected marker diameter after clamping, and `snapshot_steps` identifies steps larger than that diameter between continuous snapshots without an opacity fade. These masks retain events between samples. `jobs` includes crossfade copies, `min_jobs` captures a temporary count drop, and `batches`/`quads` count the player pass's submitted primitives, including trails and names. `resets` counts player-state resets, while `layer` identifies the selected map layer. The separate `radar resets` line counts HUD, initialization, video, player-clock, radar-clock, team and observer resets and includes the last reason and clock values. With recording enabled, each reset also prints its reason and message number. History remains across player-state resets and clears on video reset, a client-time rewind beyond 10 ms, or a recording gap of at least 6.4 seconds. Smaller backwards clock corrections retain the preceding radar frame time. Use `hud_radar_debug 0` to stop recording.

Structure/determinism and developer visual/semantic acceptance are separate results. Fake GL tests verify calls/resources, not pixels/drivers. Engine ordering, observer packet cadence, moving brush behavior, and actual context teardown require runtime observation. Keep session measurements and completed records under `.superpowers/reports/`.
