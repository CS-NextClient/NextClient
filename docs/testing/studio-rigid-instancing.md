# Verify Studio rigid instancing

[Russian version](studio-rigid-instancing.ru.md) | [Renderer architecture](../renderer-architecture.md) | [Diagnostics](../renderer-diagnostics.md) | [Testing index](README.md)

Rigid instancing draws several instances of a model together. Verify that drawing repeated models together preserves the image and reduces OpenGL calls. Models that cannot be combined must still render completely. Compare CPU and GPU time to assess performance; fewer calls alone do not establish a speedup.

## Prepare the comparison

Use a fixed scene or recorded demo with repeated visible Studio models. Record the source revision, installed modules, GPU/driver, resolution, shader/material/queue settings, and monitor configuration. Prepare content with the [Studio tools](../tools/studio-content.md) and install a matching module/resource set using the [installation procedure](installation-and-factory.md).

Use repeated models with ordinary built-in materials for the batching case, then include authored materials and other fallback cases. The diagnostic counters explain why a scene does not batch. The [renderer architecture](../renderer-architecture.md#engine-integration) describes the compatibility requirement.

## Observe drawing and counters

1. Enable `r_studio_shader 1` and `r_studio_queue 1`. Keep material settings appropriate to the case.
2. Run `r_studio_rigid_stats reset` immediately before the fixed segment, then `r_studio_rigid_stats` after it. Save the output with the scene settings.
3. Repeat the segment with `r_studio_queue 0`. This disables both queuing and combined model drawing. The timing difference reflects both features and does not isolate the effect of combining models.
4. Compare captures using [image comparison](../tools/image-comparison.md) and inspect the cases below. Repeat timing runs with the same warmup and capture settings.

| Case | Pass condition |
|---|---|
| Repeated eligible models, including different body selections and poses | The counters report groups of instances drawn together and saved calls. Geometry and poses match the result with `r_studio_queue 0`. |
| Transparent, additive, masked, and fogged models | Order, coverage, color, and depth match the reference. |
| State change between adjacent models | Models with different drawing settings are not combined in one call. The image matches the reference. |
| Authored materials or other unsupported inputs | The fallback path renders the complete model without duplicate or missing geometry. |
| Map reset, context recreation, and return to the scene | Rendering resumes with current resources and correct poses. |

`physical_draws` counts GL API calls; one multidraw call counts once. The group and triangle counters describe the geometry selected for drawing. `submitted_triangles` also includes triangles sent to OpenGL and discarded during instanced drawing. `saved_calls` reports how many drawing calls were avoided by combining models. Compare counts over the same segment after resetting them; these counters are cumulative.

## Measure the cost

Use `cl_perf_batch 2` for detailed Studio CPU timing. In Tracy, `Instance planning`, `Instance upload`, and `Instance submission` separate the corresponding costs. Record those diagnostics separately from a production timedemo with profiling and phase capture disabled.

For a frame-aligned comparison, capture the same demo-frame range, including both endpoints. Use `r_studio_phases_start <name> <first_demo_frame> <last_demo_frame>`, then stop the capture and export it. The [phase diagnostics](../renderer-diagnostics.md#recording-rendering-stages-during-demo-playback) define the commands, fields, completion marker, and overflow handling. Compare only records with `complete=1` and matching demo-frame numbers. Retain both CPU/GPU measurements and draw counts.

## Automated checks

Configure an x86 test build and build each affected target separately. For the tracked Ninja test preset:

```text
cmake --preset ninja-tests
cmake --build --preset ninja-tests-release --target studio-capture-gl-tests
cmake --build --preset ninja-tests-release --target studio-pose-tests
cmake --build --preset ninja-tests-release --target studio-phase-capture-tests
ctest --test-dir build/ninja-tests -C Release -R "^(StudioCaptureGlTest[.]Rigid|StudioPoseTest[.]|StudioPhaseCaptureTest[.])" --output-on-failure
```

These tests compare appearance, object order, poses, recovery after graphics changes, and capture export. GL checks require OpenGL 3.3 compatibility support. A skipped case does not establish rendering correctness. Record automated results separately from installed-engine appearance and performance.
