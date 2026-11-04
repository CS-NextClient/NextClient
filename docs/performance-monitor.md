# Performance monitor

[Russian version](performance-monitor.ru.md) | [Architecture](performance-architecture.md) | [Reading graphs](#reading-graphs) | [Verification](testing/performance-monitor.md)

The monitor shows where the game spends time: engine processing, rendering, radar, and other operations. It also helps inspect network traffic and save diagnostic data. The monitor is available in Release builds.

## Getting started

Open the game console and enable the graphs:

```text
cl_perf 2
```

The panel appears in the upper right. Compare results using the same scene, resolution, FPS limit, and VSync setting. The [Reading graphs](#reading-graphs) section explains values, scales, and statistics.

## Panel controls

| Setting | Values |
|---|---|
| `cl_perf` | `0`: main monitor off; `1`: numeric tables; `2`: combined CPU/GPU graphs; `3`: a separate graph for each metric. Default `0`. |
| `cl_perf_gpu` | `0`: GPU measurements off; `1`: enabled when supported. Default `1`. |
| `cl_perf_net` | `1`: network statistics. Default `0`. |
| `cl_perf_map` | `1`: map geometry and coordinates; `2`: properties and outline of the selected object in a local game. Remote servers use mode 1. Default `0`. |
| `cl_perf_batch` | `1`: model batching counters; `2`: counters and detailed CPU measurements. Default `0`. |

Network, map diagnostics, and batching counters also work with `cl_perf 0`. With `developer 1`, the main and pause menus have buttons for switching these modes.

`cl_perf_scale` sets the panel scale from 0.5 to 3; `0` selects it automatically, and the default is `1`. `cl_perf_background_alpha` sets background opacity from `0` to `1`, with a default of `0.2`. Window size affects panel placement and the number of visible measurements.

## Reading graphs

The illustrations use test data at 1600x1200 with `cl_perf_scale 1`.

![CPU and GPU measurement groups](images/performance-monitor/cl-perf-2-example.png)

### Measurement groups

| Metric | Measurement |
|---|---|
| Swap Interval | Time between image updates in the main window, including waits caused by the FPS limit and VSync. |
| Host Frame | All engine work per frame: game data processing, rendering preparation, and waits. |
| Studio and Radar under CPU | CPU time spent on models and radar. |
| Swap | CPU time spent presenting the finished image; it can include waiting for the GPU or VSync. |
| Render | GPU rendering time, including the monitor panel. |
| Studio and Radar under GPU | GPU time spent rendering NextClient models and radar. Stock model rendering is measured only on the CPU. |

On the CPU graph, the blue bar shows Swap Interval and the orange bar shows Host Frame. Colored segments show Studio, Radar, and Swap. On the GPU graph, the translucent bar shows Render, with colored segments for Studio and Radar.

An operation's time is already included in its total, so do not add these bars or rows. CPU and GPU work concurrently, so adding their times does not give the frame time either.

The Monitor row shows average CPU time spent on the monitor itself, including panel drawing and retrieving GPU results. The average uses the frames shown on the graph.

Worker shows background radar analysis time. It runs concurrently and is not added to the main frame time.

### Frames on the graph

`History | last N frames` means the last N frames are shown. New values appear on the right and move previous values to the left. Each bar is one physical pixel wide. Changing the panel size or scale changes N and the data used for statistics.

Each image update occupies a separate graph position. If the engine updates the screen several times in one game update, its CPU time appears once, at the last image update. If the screen was not updated, the engine measurement gets its own position. The number of bars can therefore differ from the number of engine updates.

Tables use the same N frames as the graphs. At low FPS, these frames can cover more than ten seconds. Empty space on the left means there is not yet enough data to fill the graph.

When some measurements are pending, skipped, or incomplete, the corresponding stacked bar is omitted. An inactive part contributes zero. A very short operation can be less than one pixel high and remain invisible.

CPU/GPU graphs and their tables refresh every 250 ms. The vertical axis shows milliseconds. The combined CPU/GPU graphs share a scale based on the largest values, with a minimum range of 1 ms. A large peak can make other bars difficult to see. Use `cl_perf 3` to inspect small values with a separate scale for each metric.

![Separate measurement graphs](images/performance-monitor/cl-perf-3-example.png)

### Statistics

CPU/GPU tables show `avg`, `p99`, and `max` for the selected N frames. Skipped, pending, and inactive measurements are excluded.

| Column | Meaning |
|---|---|
| `avg` | Arithmetic mean. |
| `p99` | A value at or above 99% of measurements. It uses the nearest-rank calculation and appears after 100 measurements of that metric. Until then, the table shows `-`. |
| `max` | Largest value. |

Each row describes its own operation. For example, average Studio time is already included in average Host Frame time, so do not add the rows. When an operation has no measurements, the table marks it inactive.

`perf_dump` uses a separate history covering the last 10 seconds. It reports `p95` and `p99` for Swap Interval. `p95` is a value at or above 95% of measurements. `percentiles=1` appears when this history contains at least 100 Swap Interval measurements.

Header FPS uses the last 0.5 seconds: the number of positive intervals between screen updates divided by their total duration. Header FPS and table statistics can therefore describe different periods. For comparison, 60 FPS corresponds to 16.67 ms, 100 FPS to 10 ms, and 144 FPS to 6.94 ms.

### Why GPU results arrive later

The GPU can finish its work several frames after the CPU. Its graph position stays blank until the result arrives. The result remains at the original frame instead of moving to the time it became available.

Insufficient measurement capacity can also cause gaps. Check reports of skipped measurements before comparing results.

GPU age shows how many image updates have passed since the frame with the latest available GPU measurement. Zero does not by itself mean a measurement is available; also check that a result is present.

Render time can include pauses while the GPU waits for more work from the CPU. Disabling radar blur can therefore reduce Radar time without an equal reduction in Render. Check Backdrop capture and Backdrop blur in `perf_dump` and the CPU/GPU timeline in Tracy to investigate.

Swap alone cannot identify whether a wait came from the GPU, VSync, or the driver. Compare it with Swap Interval, Host Frame, Render, and the current VSync and FPS settings. `cl_perf_gpu 0` disables built-in GPU measurements.

### Packet-based network graphs

`Network | last N packets` means the horizontal axis counts packets. Each position is one physical pixel wide. New data appears on the right; changing the panel size changes N.

Incoming and outgoing packet sizes use separate sequences. Latency, loss, and choke use the incoming packet-number sequence. Matching horizontal positions on different graphs can therefore refer to different times.

| Metric | Meaning |
|---|---|
| Latency | Delay in milliseconds, measured after processing an ordinary server message. A message assembled from reliably transmitted fragments does not produce a separate sample. |
| Packet size (B) | Incoming (In) and outgoing (Out) packet sizes in bytes as reported by the engine. |
| Traffic | KiB/s and packets per second, sampled at 100 ms intervals. |
| Loss | Lost packets, shown as red marks below the latency graph. |
| Choke | Sending skipped because of transmission rate limits, shown as orange marks. |

Network tables use `avg`, `median`, and `max` for the displayed N packets. `median` is the middle value in sorted order. Loss and choke totals are percentages. Positions marked with loss or choke have no latency sample.

If the engine has already discarded an older packet record, the graph has a gap. A gap is neither zero latency nor a confirmed loss. New network data appears at the next screen update. The bottom of the separate network panel reports the number of retained In and Out positions.

## Capturing a frame

```text
perf_dump
```

This command measures the next completed frame and prints detailed CPU, GPU, network, and map data to the console. It also works with the main panel disabled. If an error interrupts the frame, the command waits for the next one. A map change, monitor reset, or game shutdown cancels the request.

In CPU rows, `incl` (inclusive time) includes called operations, while `self` excludes their measured duration. GPU data can refer to an earlier frame. Preserve the frame numbers printed in the dump when comparing it with a Tracy recording.

`perf_reset` clears measurement history and map diagnostic data. Capture detailed dumps separately from ordinary performance measurements: collecting a dump also takes time.

## Studio batching counters

Batching draws several models or model parts together to reduce graphics API calls. `cl_perf_batch 1` displays the counters; `cl_perf_batch 2` also measures CPU time. The table covers up to eight recent completed frames, including frames without Studio models.

| Value | Meaning |
|---|---|
| Counter totals and Calls total | Sum over the displayed frames. |
| `ms/frame` | Average operation time per frame. |
| `us/call` | Average time per operation call. |
| Queue peak | Maximum draw queue size during the latest opaque rendering pass; zero when the queue was empty. |
| Saved draw calls | How many calls batching removed. Verify actual speed improvements using CPU/GPU time. |

The [renderer diagnostics guide](renderer-diagnostics.md) describes the comparison procedure. A single-frame dump and a table covering several frames can show different values.

## Network and map diagnostics

`cl_perf_net 1` displays packet sizes, latency, traffic, loss, and choke: packets the server skipped sending because of transmission rate limits. When the panel fits in the window, it hides the stock network graph while preserving its setting. See the [network graphs](#packet-based-network-graphs).

`cl_perf_map 1` displays geometry counters and coordinates. Mode `2` adds properties and an outline for the selected object in a local game. Commands are described in the [map diagnostics guide](performance-limits.md).

## Recording with Tracy

Configure a profiling build and build each required module with a separate command:

```text
cmake --preset ninja-profiler
cmake --build --preset ninja-profiler-release --target engine_mini
```

Prepare a separate game installation for diagnostics. Launcher, NitroApi, engine, client, and the Tracy library must use compatible build settings. This preset writes files to `build/ninja-profiler/out/bin/Release`. Building one module does not install the complete set; follow the [installation procedure](testing/installation-and-factory.md).

Use Tracy viewer version 0.14.1 to match the client library in the repository. Connecting the viewer enables recording; `perf_dump` reports `trace=1`. The built-in monitor settings remain independent.

Find a slow frame in the recording and compare its CPU and GPU operations. Host labels identify engine work per frame; Output labels identify image presentation. Nested operations are already included in their enclosing operation, so do not add their durations. `Monitor` labels show diagnostic overhead.

Save the recording in the viewer. If recording stops because of its memory limit, restart the game before making another recording. The game and built-in monitor continue running. `perf_dump` reports the stop reason; a crash can leave an incomplete recording.

Use Tracy to find slow operations. Compare final performance with profiling disabled, using the same scene and settings. The [test procedure](testing/performance-monitor.md) lists verification scenarios.
