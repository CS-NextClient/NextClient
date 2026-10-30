# Studio renderer diagnostics

[Russian translation](renderer-diagnostics.ru.md) · [Architecture](renderer-architecture.md) | [Verification cases](testing/studio-rendering.md)

Run these commands manually on a loaded map in the installed game. Before inspecting a new build, prepare and install a matching set of modules and resources using the [installation procedure](testing/installation-and-factory.md).

## Select the path

```text
r_studio_shader 1
r_studio_materials 1
r_studio_queue 1
r_studio_info
```

The first command enables shader submission when an OpenGL 3.3 compatibility context is available. The second enables authored materials. Both settings default to `1`. Setting `r_studio_materials 0` keeps GPU skinning but suppresses authored materials. Setting `r_studio_shader 0` selects the engine's submission path.

`r_studio_queue` defaults to `1` and shares one GL state scope across consecutive eligible Studio packets in each opaque or transparent entity traversal. `r_studio_queue 0` keeps shader rendering with immediate packet submission. Use it to compare queue overhead and for custom studio callbacks issuing raw OpenGL without explicit queue barriers. Engine remap skins always use immediate submission.

Within each submission series, the renderer caches blend enable/factors/equations, depth test/write/function/range, cull enable/face, alpha test/function, and the base unit-0 texture binding. Packet defaults and masked/additive overrides update the same cache. Every series starts with unknown cached values. Caller restoration remains unconditional, and each packet keeps its program bind, uniforms, matrices, and bone upload.

`r_studio_info` prints the last captured studio model, converted geometry counts, observed submission counts and the rejection status of the most recent submission attempt. A submission is one intercepted `StudioDrawPoints` call, normally for a body part. The command also prints packet count, maximum run length, studio program binds and nonempty flushes by reason: brush, fallback, client, stage_end, capacity, resource, immediate and remap. Interception and shader-acceptance counts accumulate while the module runs. Queue statistics start at the latest opaque traversal and include the subsequent transparent traversal and immediate submissions until the next opaque traversal.

Program binds exclude restoration of the caller's program and unrelated shader work. Each packet retains its first program bind for compatibility drivers, along with its independent uniforms and bone upload.

Compare a stationary scene with many visible opaque Studio models, such as the north room of `studio_test`, with `r_studio_queue 0` and `1`. Keep the view, shader/material settings, and monitor configuration unchanged. Check packet/run counts and studio program binds, then compare CPU/GPU time. A remap flush submits the queue before a model with a mutable skin. A resource flush submits it before resources change.

`cl_perf_batch 1` shows grouped/single model draws and mesh ranges plus draw-call savings. It works independently with `cl_perf 0`: only counters are collected, without continuous built-in CPU scopes or GPU queries. The heading marks totals over up to eight recent completed accepted Host frames, including empty frames. `cl_perf_batch 0` hides the block separately.

`cl_perf_batch 2` measures CPU costs continuously and updates the same window of up to eight frames after each completed accepted Host frame, including with `cl_perf 0`.

The CPU heading identifies averages: `ms/frame` is the operation's total time divided by the window frame count; `us/call` is that time divided by the operation call count. `Calls total` is the sum of calls over the window. The level-2 table shows save/restore, program and buffer binds, and bone uploads. `Top detail` identifies the most expensive measured detail when a sample is available.

`perf_dump` independently captures the next complete accepted Host frame and prints automatically after it finishes. Without a connected Tracy viewer, detailed Studio timings are dormant between requests at batching levels 0/1; level 2 keeps them active. CPU-only capture with `cl_perf 0` does not enable GPU queries. Its saved one-frame result remains separate from the continuously updated batching table. Nested save/restore phases and five Studio preparation/submission details are available in the dump. See [Studio batching counts](performance-monitor.md#studio-batching-counts) for units and measurement boundaries.

## Capture a frame description

```text
r_studio_dump render_check
```

This arms recording for the next scene and writes `<gamedir>/render_check.txt`. The dump records submissions, selected path, fallback reasons, mesh/material counts and lighting/render-mode inputs.

```text
r_studio_dump render_lights lit
```

The `lit` form waits for a frame with an entity light so the dump includes its lighting inputs.

## Compare images

Hold the scene still, then run:

```text
r_studio_ab render_compare
```

The command captures the engine and shader paths in two frames, writes `render_compare_off.tga` and `render_compare_on.tga` under `<gamedir>`, and restores the saved shader/material settings. Authored materials are disabled for both frames so their appearance does not dominate the comparison. Movement between frames can produce differences unrelated to rendering. The command requires a loaded map and refuses another capture while one is pending.

Account for the shader path's fullbright behavior and skipped stock planar shadows when interpreting an image difference. See the architecture reference for these limits.

## Interpret performance counters

```text
cl_perf 3
perf_dump
```

The monitor exposes these separate counters:

| Counter | Meaning |
|---|---|
| Studio shader submissions | Intercepted body-part submissions accepted by the shader path. |
| Studio engine submissions | Submissions delegated to the stock engine. |
| Studio draw calls | Calls to `glMultiDrawElements` made by the shader path. |
| Studio constituent draws | Total index ranges passed inside those calls. |
| Studio triangles | Sum of submitted index counts divided by three. |

For a measured sample with nonzero `Studio draw calls`, the ratio of constituent draws to draw calls gives the average number of ranges per MultiDraw call. A call with 6 and 9 indices counts as one API call, two constituent draws and five triangles.

Studio CPU includes preparation and submission work. Studio GPU timing surrounds shader MultiDraw calls. Counter reductions alone do not establish an FPS improvement. Compare CPU and GPU time under the same scene, materials and monitor configuration.

Runtime cases and automated coverage are listed in [Studio rendering verification](testing/studio-rendering.md).

## Embedded shader preparation

Shader sources, shared GLSL files and manifests are embedded in the module. Programs compile after GL context creation and remain in memory for that context.

Use the launch argument `-dev` for startup compilation diagnostics. `-noshaderwarmup` defers compilation until a program is first used.
