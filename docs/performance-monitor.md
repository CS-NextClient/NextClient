# Performance monitor

[Russian translation](performance-monitor.ru.md)

For axes, colors, `avg`/`p99`/`max`, and examples of slow-frame interpretation, see [Reading performance graphs](performance-graphs.md).

## Getting started

The built-in monitor is available in Release builds. Enable it in the game console:

```text
cl_perf 2
```

CPU and GPU plots appear at the upper-right corner. Set `cl_perf 1` for numeric tables or `cl_perf 0` to disable the main monitor. Network, map inspection, and batching blocks have separate controls, described below.

## Panel controls

| Setting | Values |
|---|---|
| `cl_perf` | `0`: performance collection disabled; `1`: numeric tables; `2`: grouped CPU/GPU column plots; `3`: eight individual plots. Default: `0`. |
| `cl_perf_gpu` | `0`: GPU measurements disabled; `1`: GPU measurements enabled when supported. Default: `1`. |
| `cl_perf_net` | `1`: network statistics enabled. Default: `0`. |
| `cl_perf_map` | `1`: polygon and brush counts, position, angles, and look coordinates; `2`: object name, properties and outline in a local `map` game, otherwise mode 1. Default: `0`. |
| `cl_perf_batch` | `1`: Studio batching summary; `2`: summary and detailed CPU costs. Default: `0`. |

Network statistics, map inspection, and Studio batching also work with `cl_perf 0`. These settings apply to the current game session. With `developer 1`, the upper-left tools group in the main menu and pause menu provides Performance, Network, Studio batching, and Map inspection buttons. Each click cycles through the setting's supported values in the table; the caption shows the current mode. Console changes update the captions immediately. The indicator shows the current `developer` value; `developer 0` hides the group. The Material editor button is enabled from the disconnected main menu.

`perf_dump` captures the next complete accepted Host frame and automatically prints its CPU details with performance, network, and mapper snapshots; `perf_reset` clears their histories and scene baselines.

## Scale and appearance

`cl_perf_scale 1` is the default base scale, using 14-pixel text. Positive values select a multiplier from 0.5 to 3; `cl_perf_scale 2` uses 28-pixel text. `cl_perf_scale 0` selects automatic scaling from drawable height with 1600x1200 as the reference, capped at twice the base scale. Text, spacing, and graph fields follow the scale, and the panel remains clipped to the window. CPU/GPU frame columns and network packet columns stay one physical pixel wide; their window sizes follow the scaled field widths.

## Frames and measurements

GoldSrc processes a frame in an outer `Host_Frame` call. The monitor calls it an accepted Host frame once `Host_FilterTime` allows processing to continue. An output is a buffer swap through `SDL_GL_SwapWindow` for the main window. One Host frame can produce several outputs, and an output can also occur outside Host processing.

Swap Interval is the elapsed time between completed main-window swaps. Header FPS uses positive intervals completed within the last 0.5 seconds and refreshes every 250 ms. Its value is the interval count divided by their total duration. Panel duration tables use the current N-frame window; the FPS in `perf_dump` uses its diagnostic time history. Host Frame covers accepted processing through the return from the outer `Host_Frame` call and its subscribers. Swap covers buffer exchange within accepted Host frames, accumulating multiple swaps.

Render covers GPU screen commands through the panel before presentation. Studio and radar CPU/GPU measurements can overlap the enclosing frame measurements.

All three performance modes show `History | last N frames`: tables and plots use the same latest N chronological frame slots. N follows the physical panel-field width and can change with mode, scale, or resolution. CPU/GPU slot order, gaps, and delayed results are described in the [graph reference](performance-graphs.md#frame-columns-and-statistics).

Main numeric summaries refresh every 250 ms. The batching table refreshes for each completed Host frame. Map inspection follows the latest rendered view, and new network data appears at the next presentation. The panel draws at every presentation using the latest values for each section.

CPU/GPU `avg`, `p99` and `max` use valid measured samples inside the current N-frame window. Missing, pending and inactive values are excluded. Nearest-rank p99 appears after 100 valid measurements of that series within the window. The retained history is bounded by record count; at low FPS, the N slots can span more than ten seconds. The Monitor CPU footer uses the same N-frame window.

Diagnostic `perf_dump` history summaries and FPS use a separate ten-second window; its Swap Interval p95/p99 and percentiles flag describe that diagnostic history. The monitor checks for ready GPU results without waiting. Pass rows show the output identifier associated with each result and the number of presentations since that output.

CPU/GPU duration tables show `avg`, `p99`, `max`. Parent measurements include their measured components, so overlaid columns and table rows must not be added to obtain a frame total. The complete rules for the grouped CPU/GPU view, missing data, delayed GPU results and individual plots are described in [Reading performance graphs](performance-graphs.md). The Monitor footer reports average monitor overhead over the same N-frame window; stage trees, GPU passes, counters and worker reports remain available in `perf_dump`.

`Measured` includes valid zero-duration measurements. `Inactive` means the stage did not execute. `Pending` means a GPU result is still outstanding. `Unavailable` means the measurement is unsupported or its start and end points have not been identified in the engine. `Dropped` identifies exhausted collection capacity; `Incomplete` identifies a partial measurement.

All performance modes, Studio batching, network statistics, and map inspection share accented section titles, shaded table headers, consistent spacing, and separators before summary or footer rows. History and diagnostic notes use muted text. Series colors identify the same metrics in numeric rows and plots. Numeric mode shows compact `avg`, `p99`, and `max` tables.

The panel sits at the upper-right edge. `cl_perf_background_alpha` controls backing opacity from `0` (transparent) to `1` (opaque), with a default of `0.2`. It also scales graph backdrop opacity and takes effect immediately. Values outside the range are clamped; nonfinite values use the default. Section and table headers retain their accent colors. Windows text uses antialiased Consolas at native pixel size with a one-pixel shadow. The panel draws before the main-window swap, including menus and `hud_draw 0`. Its own CPU cost appears as Monitor CPU.

`perf_dump` CPU rows display inclusive time, self time, and call count. GPU pass rows describe command intervals, including GPU idle time between commands. Shader compile and link durations show the most recent measured call of each operation.

`CPU frame` identifies the completed Host record represented by the CPU rows. `Output` and its `Host` identify the most recent presentation; a screen-only output has Host zero. These identifiers may differ while the next Host invocation is still running. An unmeasured shader build is `Unavailable`; a measured zero duration is `Measured`.

A `Host_EndGame` or `Host_Error` abort discards the unfinished Host measurement and its open CPU/GPU scopes. The outer `Host_Frame` boundary restores collection for subsequent frames. A pending `perf_dump` request retries on the next complete accepted Host frame.

Map changes invalidate pending GPU results and clear current feature rows while retaining process history. A resize preserves query objects in the same GL context. Context replacement starts a new context generation; old query names are abandoned when their owning context is no longer current. Cadence continues across a context change while the SDL window remains the same, preserving loading pauses.

## Studio batching counts

The batching block is controlled independently of the performance graphs:

```text
cl_perf 0
cl_perf_batch 1
```

`cl_perf_batch 1` shows a seven-line summary of up to eight recent completed accepted Host frames. `cl_perf_batch 2` continuously measures detailed Studio CPU costs over the same window, including with `cl_perf 0`. Its five categories use a fixed alphabetical order by CPU operation. Both levels update at each presentation using the latest completed Host frame. Empty frames count toward the window; rejected Host calls and standalone outputs do not. The heading shows the actual number of retained frames while the window fills.

Level changes, map replacement and reset clear this window. A frame containing a level change is excluded. Level 0 hides the block and is the default. Level 1 adds no detailed CPU timing, and neither level enables GPU queries when `cl_perf 0`. Detailed timings add collection cost while level 2 is enabled.

Run `perf_dump` in the scene to measure the next complete accepted Host frame. The command prints automatically before the next main-window buffer swap after that Host frame finishes. The capture works at every `cl_perf` and `cl_perf_batch` level and preserves their settings. Repeated requests do not restart an outstanding capture. `perf_reset`, map replacement and shutdown cancel the request and clear its saved result. A rejected or nested Host invocation cannot finish it.

The CPU capture is labeled separately from the continuous history and asynchronous GPU results. Host frame callbacks and main-window presentation must be available; otherwise the command reports that capture is unavailable.

Wide windows show the batching block to the left of the main panel. The detailed block has 17 lines; narrow or short windows retain the summary and the highest measured detail (or the highest broad GL cost while detail samples are absent). The main performance modes keep their own layout. `cl_perf 0` disables their collection; the batching block remains visible while `cl_perf_batch` is nonzero. Set `cl_perf_batch 0` to hide it. It also coexists with the independent network and map inspection panels.

The table shows `Grouped`, `Single`, their `Total`, and the grouped share. The heading marks counts as totals over the displayed frame window. Shaded headers and horizontal separators distinguish the count table, draw-call savings, CPU costs, and CPU summary. The rows describe different measurements:

| Field | Meaning |
|---|---|
| Model draws | Packets containing copies of meshes, pose, and model parameters. `Grouped` packets share a submission batch with another packet; `Single` packets are submitted alone. `Grouped %` uses all submitted packets. One entity can produce several packets. |
| MultiDraw meshes | Compatible mesh ranges inside one packet. `Grouped` ranges belong to calls with at least two ranges; `Single` ranges use single-range calls. The percentage uses all shader mesh ranges. |
| Draw calls saved | Mesh ranges minus actual shader draw calls, divided by all mesh ranges. This measures the reduction in API calls. A high packet grouping percentage does not imply the same draw-call reduction. |

A submission batch saves and restores the caller's GL state once. Programs and buffers can change inside it, and each packet retains its own pose and uniforms. Counters cover the accepted Host frames shown in the heading, up to eight frames. The main performance tables and graphs use their shared latest N-frame window. An absent denominator shows `-`.

Level 2 separates these nonoverlapping CPU categories:

| Panel row / dump stage | Measured work |
|---|---|
| GL save / GL state save | Packet raster and matrix capture, plus caller-state capture and GL stacks at batch start. |
| GL restore / GL state restore | The complete batch-end restoration, including program, VAO, buffer, texture and matrix restoration. |
| Program bind | Actual `glUseProgram` during submission. Cached skips and caller-program restoration are excluded. |
| Buffer binds | The bone UBO base bind at batch start and changed VAO binds during submission. Restorative binds belong to GL restore. |
| Bone upload | `glBufferSubData` for the captured model bone count; packets containing a material program other than `studio_legacy` retain the full 128-bone transfer. |

`ms/frame` is the category's inclusive CPU total divided by the displayed CPU frame count. `us/call` is its total divided by its call count; `Calls total` is the sum of calls over the CPU window. The CPU heading marks the two timing columns as averages. The row with the largest measured total cost per frame is highlighted. Row order follows the operation name and remains stable when durations or sample availability change. `CPU live` identifies the current short window.

Each newly completed Host frame updates its totals; `CPU live: waiting for frame` indicates that no complete frame has been collected yet. The saved one-frame `perf_dump` result remains separate from the continuously updated table.

`perf_dump` records these detailed CPU stages during its one-frame capture:

| Parent | Detail | Boundary |
|---|---|---|
| GL state save | Model state capture | Raster settings and matrices captured for model rendering. |
| GL state save | Batch state save | Caller bindings and GL stacks saved before submission, plus material texture bindings captured before their first mutation. |
| GL state restore | Restore bindings | VAO, bone UBO, general UBO, program and array-buffer restoration. The array-buffer restoration keeps its original position after attribute restoration, so this category has two calls per batch. |
| GL state restore | Restore matrices/attributes | Matrix stacks, matrix mode and `glPopAttrib`. |
| GL state restore | Restore textures | Material sampler bindings and the engine's base texture binding. |
| Studio | Model data preparation | Captured bone/model/light inputs, mesh records and CPU submission range arrays. |
| Studio | Mesh sorting | Creation of the mesh index order and its stable sort. |
| Studio | GL state apply | Raster settings and matrices applied during model/group submission. |
| Studio | Uniform setup | Standard model/group uniforms and program configuration. |
| Studio | Texture setup | Base-skin binding before drawing and unbinding material textures afterward. |

The five direct Studio details exclude queue drains, geometry/material preparation, MultiDraw and state save/restore. Authored material uploads keep their separate Material preparation measurement. Unclassified work remains in Studio self.

The compact panel adds only `Top detail`, the largest valid detail total per CPU frame. The complete detailed list stays in `perf_dump`; level 1 does not display it. An explicitly connected external trace continues collecting its instrumented scopes. `Other Studio` adds the five direct Studio detail totals back to the remaining Studio self time. Nested save/restore details are excluded from this sum, and parent/child totals must not be added together. Geometry, material preparation and Shader submission remain separate children in the dump.

Shader submission covers MultiDraw and its accounting. Missing or incomplete measurements show `-`; measured zero remains `0.000`. These are CPU elapsed times, including driver waits and instrumentation cost.

The dump reports what triggered queue submission or immediate packet submission. It also records batch sizes, program binds, stock renderer calls, and all five CPU stages. Immediate causes count packets; flush reasons count batches and can end a large shared batch. `r_studio_info` uses a separate interval starting at the latest opaque traversal. See [studio diagnostics](renderer-diagnostics.md) for controls and comparisons. The public interface is `PerformanceMonitor001`; rebuild the engine and client together.

## Optional network and mapper blocks

```text
cl_perf 2
cl_perf_net 1
cl_perf_map 1
```

`cl_perf` controls the performance panel. Network (`cl_perf_net`), Studio batching (`cl_perf_batch`) and map inspection (`cl_perf_map`) collect and display independently, including with `cl_perf 0`. Panel scale and backing opacity also apply when performance is disabled. The network panel keeps its own width and packet-window size across performance modes.

Each enabled monitor has its own backing and a visible gap from adjacent panels. Panels start at the upper-right corner and fill downward in this order: performance, network, Studio batching, map inspection. A section that does not fit moves to the top of the next column on the left; later sections do not move back into earlier columns. The viewport width limits the available columns. In the final column, the layout reserves basic rows for the remaining panels and gives the network plots priority over expanded details. Section headers remain visible when only their minimum height fits.

For a standalone network panel, use `cl_perf 0` with `cl_perf_net 1`. To restore the stock network display, set `cl_perf_net 0`; the original `net_graph` setting is preserved. The stock display also remains available when the new network block cannot fit or the required presentation path is unavailable. `cl_perf_map 0` hides map inspection and its object outline. See [Map inspection](performance-limits.md) for counter definitions and available properties.

Network graphs use the latest N packet positions, where N follows the physical plot width. In/Out sizes have independent windows; latency and loss/choke markers use incoming sequence slots. Equal horizontal positions across windows can represent different moments.

Latency and packet-size tables show `avg`, `median`, `max` for valid values in the current window. Headline In/Out values remain KiB/s and packets per second, sampled at 100 ms intervals. Axes, gaps, and loss markers are described in the [network graph reference](performance-graphs.md#packet-network-graphs).

## Diagnostic Tracy capture

The diagnostic Release configuration uses one shared Tracy 0.14.1 client. Configure `ninja-profiler` and build the required modules with `ninja-profiler-release`; its artifacts are under `build/ninja-profiler/out/bin/Release`. Ordinary Windows production artifacts remain under `out/bin/Release`. Linux artifacts use the selected configure binary directory. Each build command names one target:

```text
cmake --preset ninja-profiler
cmake --build --preset ninja-profiler-release --target engine_mini
```

Use a separate diagnostic game installation containing matching launcher, NitroApi, engine, client, and `TracyClient.dll`. `BUILD_ALL` installs the shared client only when `USE_PROFILER=ON`. The external viewer must match the pinned client version and runs separately from the x86 game. Viewer connection activates on-demand CPU zones, numeric values, and native GPU capture. `cl_perf 0`, `cl_perf_gpu 0`, and `perf_reset` independently control the built-in monitor. `perf_dump` reports `trace=1` while a viewer is connected.

Every available CPU stage from `perf_dump` is a native Tracy zone, including all detailed Studio save/restore, program, buffer, bone-transfer, preparation, sorting, state, uniform, and texture stages. Existing GPU stages use native GPU zones. Host and Output markers correlate simulation and presentation. Connecting Tracy activates this collection automatically.

If Host processing aborts, its open CPU zones close in reverse nesting order. The trace then continues with subsequent Host calls.

CPU and GPU zones preserve the source file, function, and line where the scope starts. Studio and radar wrappers forward that location; stock-engine hooks identify their registration site in the integration code. Distinct call sites for one stage retain separate source locations. Capture a new recording with matching engine and client DLLs to use these locations.

Rendering and batching counters appear as Tracy plots with their dump names. During a viewer connection, values are published once per accepted Host frame, before its Host marker. Rendering counts reset for each frame; built-in GPU drop and expiration diagnostics retain their cumulative values. Native Tracy GPU zones and the built-in GPU collector are controlled independently.

Additional diagnostic zones separate `Studio client model callback`, `Studio client player callback`, `Studio follow traversal`, `Studio parent pose`, and `Server message parsing`. The client zones cover the complete call to the original renderer callback, including model, player, viewmodel, and preparation-only calls that reach the wrappers. Their uninstrumented remainder can include animation, bone calculation, lighting, and other client work. Bone upload remains a separate existing zone.

The follow traversal includes its parent and child callbacks. Command processing, prediction, and the closed stock engine's internal GUI operations can be measured only at available hook points.

Trace events stream to the external viewer, which retains the recording. All instrumented modules share a 128 MiB hard budget for event queue storage and associated payload buffers, including rounding to the platform's allocation granularity. Small initial queues grow as needed and reuse drained blocks. The connected worker checks for data every 2 ms while idle and flushes pending output under memory pressure.

If a stalled receiver exhausts this budget, or a budgeted allocation fails, capture stops and reconnects are rejected until the game restarts. Game execution and the built-in monitor continue. `perf_dump` reports queue usage, peak, limit, and the stop reason; `stopped=0 reason=none` identifies a usable capture. The budget covers queue storage and event payloads, while symbol metadata, fixed runtime buffers, and the rest of the game have separate allocations.

Native Host zones wrap whole Host invocations, including filtered calls. The Host frame markers identify accepted calls. The default Frames series and the named Output series identify completed main-window swaps. `Simulation delta ms` is the engine simulation delta, separate from presentation intervals. The native GPU adapter performs a one-time timestamp calibration during diagnostic GL initialization and owns a separate query pool. Collection reads only ready results; saturated pools drop new zones while preserving reserved end queries.

The launcher starts Tracy outside DLL initialization and stops it after producer workers finish, before instrumented DLLs unload. Fatal `Sys_Error` cleanup stops workers and the runtime before forwarding the fatal call; a fatal capture can contain unfinished main-thread zones. On Windows, `TRACY_NO_DBGHELP_INIT_LOAD=1` is the default when no explicit value exists, deferring eager module symbol loading until needed.

See the [developer runtime checks](testing/performance-monitor.md) for acceptance cases and coverage limits.

For live-engine validation, install `next_engine_mini.dll`, `nitro_api2.dll`, and `client_mini.dll` built from the same checkout using a production configure preset. The developer performs checks in the installed game. Test configurations build engine_mini as a static library for engine-free tests.

`perf_dump` CPU details describe the captured accepted Host frame. The diagnostic history summary reports its own frame count and interval. A row retains its actual immediate parent when all calls share it. Stages called under different parents appear as independent totals and overlap the encompassing rows.

`perf_dump` reports CPU scope-entry/exit bookkeeping for the latest completed Host and GPU-result polling for the latest output. These costs are parts of the measured frame and are not additive to Host CPU. Monitor CPU includes panel preparation/drawing and polling after swap; pre-swap polling is already inside the panel scope. Timestamp reads used to measure overhead add their own clock cost.

Tracy GPU additionally requires `glGetInteger64v` for calibration and the GL extension discovery functions. Missing entry points leave CPU tracing and the asynchronous built-in GPU collector available.

## Developer API

The public monitor type is `performance::PerformanceMonitorInterface`. Engine snapshots, counters, gauges, worker reports and scope owners belong to `performance`; shared clock, duration, sample and accounting types belong to `ncl_perf`. The [monitor owner rules](../nextclient/engine_mini/src/performance/AGENTS.md) define these boundaries.

### Ownership and calling thread

The DLL-lifetime performance core constructs its clock before the concrete final `PerformanceMonitor` and keeps `PerformanceMapper` beside them. The exported factory returns that same monitor throughout the DLL lifetime. Renderer composition supplies the actual monitor and mapper to its consumers. Same-DLL CPU/GPU scopes call the concrete monitor directly; public interface-based scopes retain the cross-DLL contract. Studio nesting belongs to each renderer and is shared by its interceptor and GL submission, so nesting on one renderer cannot suppress another renderer's measurement.

`PerformanceRuntime` receives the cached engine/client tables, optional SDL/frame-lifecycle providers, concrete monitor/mapper/clock, and the actual render device/lifecycle owners. It owns hook storage, frame subscription, network state, GPU/trace adapters, panel, snapshots, and stable cvar addresses. Ordinary C++ hooks capture that runtime. C cvar/console entry points use one explicitly bound sink. Connection-scoped inputs are read from the supplied tables at their event boundary.

`OnGameInitializing` binds the native console-text function after the engine table is copied, while localization can be absent. `OnGameInitialized` then binds validated cvars and localization. Panel construction receives these inputs, the shared Windows `TextRenderInterface`, clock, and engine texture-bookkeeping slot; drawing does not discover services. A panel created before presentation initialization receives the explicit presentation binding when that phase completes. Linux retains its console-text fallback when available.

Shutdown stops stage and swap callbacks, abandons remaining GPU resources, and requests CPU/subscription shutdown. Runtime storage, its trace backend, and the stable core survive this request. An active nested Host retains its observer until the outer exit reports completion or abort; shutdown never destroys the runtime from its callback.

Mapper views capture map, connection, and world identity plus a private session epoch before entering the stock hook chain. Map changes and `CL_ClearState` forget borrowed world data. A stale outer view returns without clearing a newer nested session snapshot. Matrix capture and outlines validate the supplied device generation and actual current context at their GL boundaries.

The `PerformanceMonitor001` interface is owned by the main engine/frame thread. All methods run on that owner thread, including snapshot reads, map-generation reads, counter and gauge writes, and `ReportWorker`. A const read does not make the API thread-safe. Worker threads return owned results; their main-thread owner publishes a `performance::PerfWorkerReport` through `ReportWorker`. CPU scopes close in LIFO order on the same owner thread. GPU scope calls additionally require that thread's matching live render context.

### Background work and radar integration

`ClientMini` owns `radar::integration::Performance` with the optional exported monitor and supplies it with the task runtime through `GameHud` to `HudRadarAdvanced`. Concrete layers, text, and map image generation receive direct dependency references. `HudRadarAdvanced` owns `radar::integration::LayerAnalysis`, which owns the native `RadarLayerAnalysisJob`.

The job accepts an owner-supplied request ID and owned geometry/overrides, then returns an owned map result with its request ID and nanosecond duration. Its optional native sink receives `Pending`, `Completed`, `Failed` and `Canceled` only from owner-thread dispatch, reset and polling. `Pending` records actual dispatch; a replaced input can be canceled before dispatch. A canceled worker cannot publish a later completion or failure.

The adapter assigns increasing request IDs, captures the monitor map generation before starting each request, translates native events into `performance::PerfWorkerReport` and removes terminal attribution. A delayed dispatch retains the generation captured at `Start`. The HUD derives diagnostic milliseconds from the result duration.

Reset cancels current requests without waiting; shutdown cancels and joins while the adapter, attribution, performance owner and scheduler remain alive. The adapter is noncopyable and nonmovable because the native sink captures its owner. Destroy the performance owner after explicit HUD shutdown and destruction, before stopping the task runtime.

`HudRadarAdvanced` owns the concrete `RadarDrawContext`, including programs, effect targets, primitive batches, GL state, and statistics. Frame preparation records the current native context/window; `GL_SetMode` and SDL context/window retirement callbacks invalidate ownership before native forwarding. Only the matching current native lifetime permits GL deletion. A lost or foreign context requires forgetting names and queued work. This decision also covers `RadarOverview`/`MapLayerRenderer` images and targets; their destructors perform no GL work. `GameHud::Shutdown` stops radar callbacks, joins analysis, and releases or abandons graphics while the monitor, scheduler, and providers remain alive. Text invalidation drops owned CPU caches; the atlas belongs to the engine.

### Metric types and publication

A scope measures one execution interval and preserves its call-site metadata. A historical duration series selects measured scope totals or presentation intervals in its descriptor's Host or Output domain. An additive counter accumulates event deltas in an accepted Host frame. An absolute gauge records an observation through `SetGauge`: another write replaces the pending value. Gauge values and summaries use `double`, independently of nanosecond durations. Each gauge descriptor defines its unit, domain, last-observation aggregation, trace name, and Studio row.

`SetGauge` accepts finite observations only inside an accepted Host frame while counter-level collection is enabled by performance monitoring, Studio batching, capture, or an external trace. An invalid identifier, nonfinite value, disabled collection, or call outside that frame returns false without replacing the pending observation. A measured zero remains a sample. No write leaves the frame's value `Inactive`; missing values do not enter scalar count, mean, or maximum.

Reset, map changes, host abort, and disabling collection clear pending observations. Only normal accepted completion commits them to history. Scalar summary `last` selects the most recent measured Host in the chosen window, while `mean` and `maximum` summarize measured per-host observations.

`performance::PerfSnapshot::gauges` contains the most recently completed accepted Host's values and states, including an inactive Host completed after an earlier measurement. `latest` continues identifying the most recent output.

Diagnostic history retains up to 16,384 Host/Output records and selects its ten-second window; reaching record capacity can shorten that coverage. The Studio table selects up to eight accepted Host frames, including inactive frames, and shows the latest Host value or state beside the selected window's `max`. The dump preserves the captured Host's gauge and reports its history count, mean, and maximum. No observation is inferred from an older measured summary.

`StudioQueuePeakPackets` measures the peak retained packet count within the last completed opaque traversal. `Studio_EndOpaquePass` first flushes pending submission and drains the queue, then publishes the retained `max_run` statistic. A completed empty traversal publishes measured zero. Several traversals in one Host replace the observation with the last completed traversal's peak. The Studio row and dump use `packets` from the descriptor. Existing packet counters keep their additive meaning.

Snapshot reads copy published state. Accepted Host completion, standalone output completion, and the presentation owner advance publication after GPU polling and engine metadata refresh. The current Host gauge refreshes at Host completion; history and CPU numeric windows retain their independent 250 ms cadence. The eight-frame Studio window also refreshes after each completed accepted Host. Measured gauge values reach the trace backend at the accepted Host commit, including trace-only collection with in-game monitoring disabled; an inactive gauge produces no trace scalar. A producer requiring only trace scalars may continue using `NCL_PROFILE_VALUE`.

## Context phase integration

`performance::runtime::ReleaseContext` releases trace and monitor GPU objects only when the monitor's stored native context matches the current context; otherwise it abandons them. `performance::runtime::AbandonContext` forgets GPU objects without GL calls, retires pending GPU values, and invalidates panel geometry while retaining completed CPU history and its accumulation window. Both phases reject stale GPU closures. Same-context resize preserves pending queries.

`performance::runtime::Shutdown` performs CPU/subscription teardown after the selected context phase and has an idempotent no-GL abandonment fallback. `performance::PerformanceMonitor::Shutdown` also abandons before CPU teardown. An active host observer and trace backend remain available until the current host exits or aborts, so shutdown cannot publish a partial host as a completed measurement. Renderer lifecycle and SDL context/window adapters select the context phase before forwarding destruction. `performance::FrameSubscription` detaches after handling the active outer Host exit, following the [NitroApi lifecycle contract](../nextclient/nitro_api/AGENTS.md#frame-lifecycle).
