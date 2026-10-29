# Reading performance graphs

[Russian translation](performance-graphs.ru.md)

The built-in monitor is available in Release builds. Enable the grouped view with `cl_perf 2`; use `cl_perf 1` for the numeric tables or `cl_perf 3` for individual plots. Settings and diagnostic capture are described in [Performance monitor](performance-monitor.md). Runtime checks are listed in [Performance monitor verification](testing/performance-monitor.md).

The images use the actual offscreen panel renderer with synthetic measurements at 1600x1200 and `cl_perf_scale 1`.

![Grouped CPU and GPU measurements](images/performance-monitor/cl-perf-2-example.png)

## Read the groups separately

The CPU table indents Host Frame under Swap Interval, with Studio, Radar and Swap beneath Host Frame. Host Frame measures accepted engine Host processing and calls to its subscribers, including render preparation, submission and waits. It is broader than an engine simulation update. Swap Interval measures time between completed main-window swaps, including pacing and waits. The indentation describes the work associated with a frame; Host and presentation boundaries do not always coincide, especially with multiple swaps in one Host invocation.

The CPU plot overlays muted blue Swap Interval and warm orange Host Frame columns from the same zero baseline. Its measured-parts column stacks Studio, Radar and Swap from bottom to top, following their normal call order. Each part comes from the same accepted Host record as the Host Frame value in that slot. Multiple swaps within that Host contribute to one Swap duration.

The GPU plot overlays a translucent Render column and the measured-parts column. Its parts stack Studio and Radar from bottom to top, using the same output record as the Render value. Studio measures shader draw calls with GPU instrumentation. Stock model rendering is measured on the CPU. Radar sums outermost instrumented radar intervals so nested map/layer/batch queries are counted once. Detailed nested queries remain available in diagnostics.

The parent and its parts overlap by definition. Never add the overlaid columns or table rows to obtain a frame total. CPU and GPU also execute concurrently. The CPU and GPU plots share one duration scale, so equal heights mean equal elapsed durations.

## Frame columns and statistics

The heading `History | last N frames` identifies the shared latest N chronological frame slots for CPU/GPU tables and plots in modes 1/2/3. Numeric mode 1 derives N from its own narrower panel field; modes 2/3 use their respective plot widths. Each slot contains one frame value per available series. CPU and GPU share the same slot order. Every presentation output occupies its own slot. If one Host produces multiple outputs, its CPU measurement and parts appear once at its last output. A Host without an output has its own slot.

New slots appear at the right edge and move earlier slots left; unfilled history stays empty on the left inside the graph.

CPU/GPU tables and graph scales use the same selected N frame slots as the plotted values. Available measurements within those slots contribute during warmup. Retained history is bounded by record count, so at low FPS the N slots can span more than ten seconds. Increasing N can include earlier retained slots.

N equals the physical plot width. Every frame column is one physical pixel wide, with no gaps between adjacent filled positions. The newest position meets the right plot edge. The background and horizontal grid span the full frame band; the vertical axis and its tick labels sit at the left edge. Scale or resolution changes can change N, and widening the graph can reveal earlier slots still retained in history. Unfilled or missing positions remain empty inside the graph. For example, a plot 423 physical pixels wide shows the latest 423 frame slots. Panel width follows the layout.

Pending, unavailable, dropped, incomplete or overlapping parts suppress the corresponding composition; a missing part is not displayed as measured zero. Inactive parts contribute zero. Durations below one vertical pixel may not produce a visible colored segment.

CPU/GPU totals use translucent filled columns. Each column has a horizontal upper edge at its frame duration. Colored parts stack within the same column and belong to the same Host or output as the total value.

CPU/GPU plots and numeric summaries refresh every 250 ms. The vertical axis uses milliseconds, with zero, half range and maximum labels when space permits. The minimum upper boundary is 1 ms. Both grouped plots use a common upper boundary selected from the displayed Swap Interval, Host Frame and Render durations. A large Swap Interval value can compress smaller costs until it leaves the graph window. `cl_perf 3` uses an independent scale per series to inspect small measurements.

![Individual measurement plots](images/performance-monitor/cl-perf-3-example.png)

## Summary statistics

CPU/GPU duration tables summarize valid measurements inside the current N-frame window using `avg`, `p99`, `max`, in that order. `avg` is the arithmetic mean of valid measured samples. `p99` is the nearest-rank 99th percentile: 99% of valid samples are at or below it. It appears after 100 valid measurements for that series inside the current window; `-` indicates insufficient samples. `max` is the largest valid sample inside the window. Missing, pending and inactive values never enter these statistics. Inactive stages are shown explicitly when there are no measured samples.

Each row summarizes its own measurements, so its statistics cannot be added to those of other rows. Network latency and packet size use `avg`, `median`, `max` for their current N-position packet windows.

`perf_dump` also reports Swap Interval `p95`, the nearest-rank 95th percentile, and `p99`. These diagnostic history summaries and dump FPS use a separate ten-second window. The `percentiles=1` flag marks percentile availability after 100 valid Swap Interval samples in that diagnostic history.

The FPS heading uses positive main-window swap intervals over the most recent 0.5 seconds: interval count divided by total interval duration. The Swap Interval table uses the current N-frame window. For reference, 60 FPS corresponds to 16.67 ms, 100 FPS to 10 ms and 144 FPS to 6.94 ms.

## Delayed GPU data and waits

GPU timestamp results arrive asynchronously, possibly several frames later. Pending or missing results leave the GPU value empty in the original output slot. A late result fills that same slot. CPU and GPU retain their common chronological positions even when only one has a value in a slot. `GPU age` counts presentations since the output associated with the latest published GPU result. Age zero by itself does not establish that a GPU measurement is available.

Render measures the GPU command interval from screen rendering through the monitor panel, ending before presentation. GPU timestamp intervals can include GPU idle time between commands. CPU durations measure wall time, including waits. GPU idle time can include gaps while the CPU prepares subsequent commands. A pass can occupy part of such a gap, so disabling radar blur can reduce Radar without reducing Render by the same amount.

An unchanged Render duration alone does not establish this cause: compare the Backdrop capture and Backdrop blur GPU rows in perf_dump and inspect the CPU/GPU timeline in Tracy.

Swap alone cannot reliably distinguish a wait for GPU completion from VSync pacing or driver behavior. Compare Swap Interval, Host Frame, Swap, Render and presentation settings. `cl_perf_gpu 0` disables GPU queries in the built-in monitor.

## Packet network graphs

`Network | last N packets` uses packet order on its horizontal axis. N equals the physical plot width: each packet position occupies one physical pixel, with no gaps. Panel scale or resolution changes can therefore change N. Each In/Out size series uses its own latest N packet positions. Latency and the loss/choke strips share a separate window of the latest N incoming sequence slots. The newest position is on the right, with unfilled history on the left inside the graph. Each window advances when its next packet or sequence arrives; idle time leaves the positions in place.

Equal horizontal positions across these windows can represent different moments. Expanding the graph can restore available history collected before the expansion. The axis, background and grid fit the full packet band.

Latency is captured after a normal server message has finished parsing. Reassembled reliable fragments do not create latency samples. Latency uses milliseconds. `Packet size (B)` shows the bytes recorded by the engine for each In/Out packet on a shared size scale. Headline traffic remains KiB/s and packets per second, sampled at 100 ms intervals. The `avg`, `median` and `max` tables summarize valid values within the current N-position packet windows. Changing N updates the statistics to match the new window. Missing values are excluded. Each valid column contains one measurement.

Network latency and packet-size plots render translucent columns. Their upper edges interpolate between adjacent positions with data. The tables show `avg` values for the current packet window.

Red loss and orange choke markers below latency identify confirmed loss/choke states at individual incoming sequence slots, using the same pixel grid as the columns. Lost or choked slots have no latency sample. Headline loss/choke values remain sampled percentages. When observation skips more entries than the engine retains, overwritten positions remain gaps: flow histories retain 32 entries per direction and the incoming frame history retains 64. Unknown entries add neither a zero measurement nor a loss/choke marker.

New network data updates the network section at the next presentation. Batching can refresh at the same presentation. CPU/GPU graphs and tables use their shared N-frame window and refresh every 250 ms. The standalone network footer shows the retained In/Out position counts.

## Detailed diagnostics

The Monitor footer reports average monitor CPU overhead over the same N-frame window, including panel work and GPU polling. `perf_dump` captures the next complete accepted Host frame and prints it at a later main-window presentation. Its CPU `incl` includes nested stages; `self` excludes measured child intervals. GPU pass rows refer to the latest output with ready asynchronous GPU results and can precede the captured CPU frame. The live batching table separately aggregates up to eight accepted Host frames. Worker duration describes background radar processing, not work to add to the main frame. Tracy provides the instrumented timeline.
