# Model rendering diagnostics

[Russian version](renderer-diagnostics.ru.md) | [Studio renderer](renderer-architecture.md) | [Test scenarios](testing/studio-rendering.md)

These commands compare NextClient rendering with the stock engine, capture images, and measure performance. Run them on a loaded map in the installed game. Before checking a new build, install the required modules and assets using the [installation procedure](testing/installation-and-factory.md).

## Selecting the renderer

```text
r_studio_shader 1
r_studio_materials 1
r_studio_queue 1
r_studio_info
```

`r_studio_shader 1` enables NextClient Studio rendering and requires OpenGL 3.3 compatibility support. `0` selects the stock renderer. `r_studio_materials 1` enables custom materials. At `0`, NextClient still renders models, but without those materials. Both settings default to `1`.

`r_studio_queue 1` allows models to be grouped for rendering. `0` disables the queue for comparison. The queue is enabled by default. Some models are drawn separately regardless of this setting, so evaluate the result using counters and CPU/GPU time.

`r_studio_info` reports the latest model, selected renderer, and model grouping. When the stock engine draws a model, diagnostics help identify the reason. For a repeatable comparison, use one scene, such as the repeated-model room on `studio_test`, and preserve the camera position and settings.

## Measuring performance

`cl_perf_batch 1` shows how many drawing operations were combined. `cl_perf_batch 2` also measures CPU operations. The panel works with `cl_perf 0`; `cl_perf_batch 0` hides it. Values are summed over up to eight recent completed frames.

`ms/frame` shows average operation time per frame, `us/call` shows average time per call, and `Calls total` shows the call count over those frames. `Top detail` identifies the slowest operation with detailed measurements.

```text
cl_perf 3
perf_dump
```

`perf_dump` captures detailed measurements of the next completed frame. At batching levels 0 and 1, detailed CPU measurements are enabled only for this request unless Tracy is connected. Level 2 measures them continuously. With the main monitor disabled, the request does not enable GPU measurements.

| Counter | Meaning |
|---|---|
| Studio shader submissions | Requests for NextClient to draw model parts. |
| Studio engine submissions | Requests for the stock renderer to draw models. |
| Studio draw calls | Graphics API draw calls. |
| Studio constituent draws | Individual model parts or instances combined in those calls. |
| Studio triangles | Triangles in the selected model parts. |

`r_studio_rigid_stats [reset]` reports accumulated repeated-model batching counters, data transfer volume, and reasons for drawing models separately. The `reset` argument clears the counters. The [repeated-model test procedure](testing/studio-rigid-instancing.md) describes the comparison.

Fewer calls do not guarantee higher FPS. Compare CPU/GPU time using the same scene, materials, and monitor settings. The [monitor guide](performance-monitor.md#studio-batching-counters) explains the panel.

## Measuring effect cost

In a profiling build, Tracy shows `ProjectBounds`: the calculation of the screen area occupied by material effects. The built-in monitor has no separate row for this measurement.

Prepare the build using the [Tracy instructions](performance-monitor.md#recording-with-tracy), connect the viewer, and play a scene with effects. Check that `ProjectBounds` appears in the recording. This measurement is unavailable with `USE_PROFILER=OFF`.

Profiler recording affects performance. Compare final FPS with profiling and rendering-stage recording disabled, using the same demo and settings and a preliminary run to load resources.

## Recording rendering stages during demo playback

Run `r_studio_phases_start <name> [first_demo_frame last_demo_frame]` before `timedemo`. Both the first and last frame are included in the range. After playback, run `r_studio_phases_stop`, then `r_studio_phases_export [name]`.

`r_studio_phases_status` reports recording activity, the number of saved results, and whether the memory limit has been reached. Map loading before recording begins does not cancel the request. A game session change or graphics restart after recording begins stops collection and preserves the data. A record with `complete=0` contains an incomplete measurement.

Collection stops at the memory limit. Before comparing results, check `overflow`, skipped records, and the stop reason. Record long demos in sections by specifying the first and last frame. Exclude incomplete measurements from comparisons.

Export creates `<gamedir>/studio_diagnostics/<name>.phases.csv` and `<name>.phases.metadata.json`. The metadata file confirms that export completed. A write failure preserves the data in memory so export can be retried. A successful export clears the recording.

CSV version `2` contains stage times, frame numbers, and model, mesh, and triangle counters. Times are in microseconds. `frame_interval_us` measures the interval between the starts of recorded scene renders; other times refer to one render. `physical_draws` counts graphics API calls, `triangles` counts selected triangles, and `submitted_triangles` counts all submitted triangles, including model parts not visible in the selected variant.

Match results by demo frame and client time. `view_id` numbers the records; `output_id=0` means an image presentation number is unavailable. The metadata file contains the demo name, recording range, completed measurement count, memory limits, skips, and stop reason.

## Saving a frame description

```text
r_studio_dump render_check
```

The command saves a description of the next scene to `<gamedir>/render_check.txt`: models, selected renderer, materials, and lighting. When NextClient delegates a model to the stock renderer, the dump includes the reason.

```text
r_studio_dump render_lights lit
```

The `lit` variant waits for a frame with a model lit by an entity light and records the light parameters.

## Comparing images

Choose a stationary scene and run:

```text
r_studio_ab render_compare
```

The command saves two images in the game directory: `render_compare_off.tga` from the stock renderer and `render_compare_on.tga` from NextClient. It then restores the settings. Custom materials are disabled in both captures. A map must be loaded, and a second capture cannot start until the first finishes.

The images come from different frames, so motion and animation can cause differences. Also account for fullbright behavior and the absence of stock planar shadows on NextClient models. See the [Studio checks](testing/studio-rendering.md#image-interpretation) for image analysis.

## Shader compilation diagnostics

The `-dev` launch option enables shader compilation messages. `-noshaderwarmup` postpones compilation until a shader is first used. It can help investigate first-use errors, but compilation during gameplay affects performance measurements.
