# Radar layers verification procedure

[Russian version](radar-layers.ru.md) | [Reference](../radar-layers.md) | [Acceptance cases](radar-layer-cases.md)

## Automated checks

Use an x86 development environment and the repository dependencies. Reconfigure after source-list changes and specify one target per build. Windows example with the tracked Visual Studio 2022 test preset:

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

Build `gameui` when shared client headers are affected. Local Ninja test presets require `NEXTCLIENT_BUILD_TESTS=ON`, an MSVC x86 environment, and an explicit build configuration. Record the compiler, preset, configuration, source revision, commands, and results.

Use `radar-gl-tests` to check marker appearance, transparency, clipping, layer visibility, and scaling. These checks need OpenGL 3.0 or newer. Linux also requires a 32-bit `libEGL.so.1` with desktop OpenGL support; CTest selects software rendering with `LIBGL_ALWAYS_SOFTWARE=1`. A graphics initialization failure is a failed check, not rendering coverage. The tests run without the game.

```powershell
cmake --build --preset vs2022-tests-release --target radar-gl-tests
ctest --test-dir build/vs2022-tests -C Release -L gl --output-on-failure
```

On Linux, run the same target in the configured x86 test directory and use `ctest --test-dir <build> -C Release -L gl --output-on-failure`. Include GL checks when changing radar appearance. Run `RadarLayerScheduledJobTest.DestructionJoinsWithoutPumpingFrames` through CTest to check shutdown with its 10-second timeout.

## Real-map checks

Provide a local BSP file and run the map acceptance case:

```powershell
$env:NEXTCLIENT_RADAR_BSP = '<absolute path to scp_foundation_bw40.bsp>'
& './build/vs2022-tests/out/bin/Release/client_mini-tests.exe' '--gtest_filter=RadarBspFile.LocalMapAnalysis'
```

If `NEXTCLIENT_RADAR_BSP` is unset or empty, the test is skipped. The map has not been verified in that case. Keep map files outside Git. Repeat for de_dust2, adv_clocktower, adv_university, and scp_foundation_bw40. de_dust2 should have one automatic layer; the other maps must retain their substantial stacked areas.

Set `NEXTCLIENT_RADAR_MESH_EXPORT` to an output JSON path to compare exported map coverage. The parent directory is created automatically. The export contains geometry and texture coordinates; use game observations to assess textures and lighting.

To check corrections, set `NEXTCLIENT_RADAR_CORRECTIONS` to a file in the [documented format](../radar-layers.md), without comments. For `cs_assault`, use `assets/cstrike/overviews/cs_assault_layers.txt` and `NEXTCLIENT_RADAR_EXPECTED_LAYERS=1`. Check warehouse interior and exterior coverage. Unset correction variables before testing other maps.

Linux x86 example with native dependencies configured:

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

Record missing dependencies and skipped checks explicitly. Compare performance using the same build configuration and preserve the diagnostic output with each result.

## Runtime evidence

The developer runs the installed game and follows the [acceptance cases](radar-layer-cases.md) in radar modes 5 and 6, including the expanded radar. Record the loaded client revision and configuration. Complete installation using the [installation procedure](installation-and-factory.md).

For each case, record the map and route, radar and observer modes, expected floor, screenshot or video, and `radar_layers_dump` before, during, and after the action. Use `pass`, `fail`, or `not_run`. A failure report includes the exact trigger and the corresponding dump fields. Field meanings are listed in the [radar reference](../radar-layers.md).

During map analysis, check changing maps, changing video settings, switching between modes 5 and 6, leaving layered modes, and exiting the client. The client must remain responsive and display only the current map's results.

For player-marker flicker, enable `hud_radar_debug 1`, reproduce the problem, and run `radar_markers_dump` immediately afterward. Preserve the output and recording, then stop recording with `hud_radar_debug 0`.

Automated results and observations in the installed game are separate evidence. Keep completed records, measurements, and logs under `.superpowers/reports/`.
