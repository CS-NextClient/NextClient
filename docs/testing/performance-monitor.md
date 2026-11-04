# Performance monitor verification

[Russian version](performance-monitor.ru.md) | [Monitor operation](../performance-monitor.md) | [Testing index](README.md)

Use these cases to verify the monitor in a developer-started game. Record the revision, installed modules, platform, GPU/driver, resolution, FPS cap, VSync, and enabled monitor modes. Keep the same scene or demo when comparing costs.

## Prepare

Install a matching module set after the [installation checks](installation-and-factory.md). For Tracy, use the profiling setup in [Monitor operation](../performance-monitor.md#recording-with-tracy). Save relevant dumps, screenshots, and recordings with each result.

## Runtime cases

| Action | Expected result |
|---|---|
| Switch `cl_perf` through 0, 1, 2, and 3 in gameplay and menus. | The requested view appears, labels remain readable, and disabling the main monitor leaves separately enabled blocks available. |
| Change resolution, panel scale, and backing opacity with several blocks enabled. | Panels stay within the window, retain distinct sections, and show consistent labels and graph scales. |
| Change the FPS cap or VSync in a fixed scene. | The monitor shows frame intervals separately from processing time. Graph and table summaries cover the period currently displayed in the graph. |
| Request `perf_dump` with the main monitor disabled, then enabled. | The console automatically prints a dump for one frame whose processing completed. Monitor settings remain unchanged. The CPU and GPU identifiers show which frames the measurements describe. |
| Reset or change maps while a dump is pending. | The pending request is canceled. A new request captures the current scene. |
| Disconnect and reconnect, including when frame processing is interrupted. | Collection resumes on subsequent frames; incomplete work is not presented as a successful complete frame. |
| Compare `cl_perf_batch` 0, 1, and 2 with `cl_perf 0`. | At `0`, the section is hidden. At `1`, it shows model-batching counters. At `2`, it also shows detailed CPU timings. The main graphs remain hidden. |
| Enable `cl_perf_net 1`, receive traffic, pause traffic, then disable it. | Packet positions advance with traffic, idle history stays readable, and the stock graph returns with its setting preserved. |
| Inspect a local map with `cl_perf_map` 1 and 2, then join an external server. | Local level 2 provides properties and an outline; an external server retains only the permitted level-1 information. |
| Use the map-inspection E action with gameplay, console, and menu input. | Gameplay prints once per press while preserving the original binding; console/menu input does not trigger inspection. |
| Change video mode and return to the same scene. | The panel and measurements resume without displaying old GPU work as a new result. |
| Connect and disconnect the Tracy viewer. | Trace collection follows the connection while built-in monitor controls remain independent. Check the dump's trace status and stop reason. |
| Exit and restart the client. | Shutdown completes and the new session starts with usable monitoring. |

For missing or delayed GPU values, check that the display distinguishes their state from a measured zero. Save the dump and recording if a value appears attached to the wrong frame. Follow [Reading graphs](../performance-monitor.md#reading-graphs) when judging overlapping durations or different windows.

For performance comparisons, repeat the same demo segment with monitoring disabled, CPU monitoring, GPU monitoring, and diagnostic capture as separate cases. Keep warmup and settings equal. One-frame dump collection and a connected profiler add diagnostic cost. Do not use these runs to measure the performance of a normal build with diagnostics disabled.

## Automated checks

Choose checks that cover the behavior being changed. Configure a supported x86 preset with tests enabled and build each selected target separately before running its CTest cases.

| Target | What is checked |
|---|---|
| `nitro_api-tests` | Detecting frame start, completion, and interruption. |
| `engine_mini-tests` | Measurement collection and shutdown, diagnostic snapshots, network data, and panel preparation. |
| `performance-history-tests` | Statistics for the selected period with missing or delayed measurements. |
| `performance-gl-tests` | GPU timing and panel rendering in a separate GL context. |
| `radar-gl-tests` | Radar rendering with GPU measurements enabled. |
| `diagnostic-profiling-tests`, `profiling-connection-tests` | Source information in profiler recordings and Tracy connection and disconnection with `USE_PROFILER=ON`. |

Monitor behavior and frame handling are described in [Performance monitor architecture](../performance-architecture.md). The [NitroApi library procedure](installation-and-factory.md) checks its public interface.

GL tests require a suitable graphics environment; a skipped test does not confirm GPU timing. Installed-game observations confirm display behavior and performance for the selected hardware and scene. Keep run results with their revision, configuration, and logs in the session report.
