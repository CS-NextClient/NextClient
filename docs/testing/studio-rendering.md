# Studio rendering verification

[Russian translation](studio-rendering.ru.md) | [Content preparation](../tools/studio-content.md) | [Capture procedures](../renderer-diagnostics.md)

The developer performs live-engine checks in the installed game. Record the source revision, installed DLLs, scene or demo, graphics settings, material settings, and monitor mode for each observation. Build artifacts and tests in a separate GL context provide separate evidence.

Prepare the affected providers, consumers and resources from one source/build configuration and verify copied hashes using the [installation and factory procedure](installation-and-factory.md). Resource lifetime follows the [renderer owner rules](../../nextclient/engine_mini/src/graphics/render/AGENTS.md).

## Runtime cases

Use `studio_test` for the flag, shell, remap, lighting, and repeated-model cases. Other scene-order cases require corresponding map content or client callbacks.

| Case | Expected result |
|---|---|
| Texture flags | Plain, flatshade, chrome, chrome-flat, masked, and additive meshes retain their intended lighting, alpha, and depth behavior. Account for the fullbright difference below. |
| Glow shells | The base pass draws before the shell. Shells retain the configured extrusion and distinct red/blue colors. A following normal pass uses its own color inputs without shell color leaking into it. |
| Entity and scene light | Models in the dark alcove respond to available lighting. Use `r_studio_dump <name> lit` to wait for an entity-light sample. |
| Carried weapon and FOLLOW | First-person and carried `p_` models retain the correct header, parent pose, attachments, and material source. |
| Remap skin | Engine and shader paths select the same remapped base texture for the chosen `colormap`. Remap models submit immediately. |
| Opaque ordering | Studio/brush/Studio order remains correct with `r_studio_queue 0` and `1`. |
| Transparent ordering and shared GL state | Compare overlapping alpha/additive Studio models with `r_studio_queue 0` and `1`, including interleaved sprite, brush and alias objects. Preserve object and mesh order, remap skins, client events and caller GL state. Eligible consecutive packets share state save/restore; external draws and pass exit drain the queue. Replay the same demo segment three times per build with matching settings and Tracy disconnected. |
| Direct-drawing boundaries | Queued packets are submitted to GL before Studio events, debug drawing, and triangle callbacks. Custom callbacks that call GL directly also flush the queue before drawing or use immediate mode. |
| Transparency | Transparent objects, masked/additive meshes, and first-person weapons retain pass ordering and GL state. |
| Materials | With the editor closed, the client selects a server binding, then a model document, then a built-in material. An active edit session temporarily overrides the selected material for its target. Values for different entities remain independent. |
| Fog | Compare engine and shader paths with the camera submerged in water. Studio models must blend with the same fog as the surrounding world. Record a mismatch separately from lighting and fullbright differences. |
| Material effects | Check the three outline modes, hidden fill, glow, dissolve, noise, and glitch using the [material scene cases](material-editor-scene.md#material-effect-checks). Repeat in windowed and fullscreen modes with MSAA Off and 4x. |
| Repeated models | Queue mode preserves each pose and uniform set. Compare packet counts, run lengths, program binds, draw calls, and CPU/GPU times in the same north-room view. |
| Resource transitions | Replacement of live geometry submits queued packets before deleting its resources. Map replacement and context loss discard pending work; subsequent draws create resources for the new map or context. |

## Image interpretation

Capture a stationary scene with `r_studio_ab flags`. From the repository root, compare the resulting files, replacing `<gamedir>` with the game directory containing the captures:

```text
python tools/radar_tests/imgdiff.py "<gamedir>/flags_off.tga" "<gamedir>/flags_on.tga" --clusters --out flags_diff.png
```

The shader path honors `STUDIO_NF_FULLBRIGHT` and skips the stock planar shadow for accepted models. Those regions can differ by design. Separate them from other differences before evaluating the result.

Inspect both the overall differing-pixel fraction and the `--clusters` report. Dense regions whose brightness changes mainly in one direction require investigation even when the overall fraction passes. Small differences along silhouettes can arise from CPU/GPU arithmetic precision. Movement, animation, lighting changes, and different hardware or resolution also affect the comparison. Preserve images and diagnostic dumps with the result.

`r_studio_dump` reports the model path, header name, selected backend, flags, and material inputs. A header name is the model's compiled name and can differ from its file path. The renderer checks model/header compatibility. The dump reports whether the shader path accepted the submission and gives the fallback reason.

## Automated coverage

| Test group | Coverage |
|---|---|
| `StudioQueueTest` | Packet order, barriers, ownership, capacity, callback pause, and immediate mode. |
| `StudioSubmissionGlTest` | Pixels for distinct poses/uniforms under one program and GL-state restoration, including additive-to-opaque transitions. |
| `StudioCaptureGlTest` | Production input capture, rollback after more than 256 meshes, material authority and choice metadata, coverage, and MSAA visibility in all outline modes. |
| `StudioCaptureWindowGlTest` | Window-context depth and MSAA visibility in all outline modes. |
| `StudioEffectsGlTest` | Silhouette and glow selection, hidden fill, mask growth, nearest-surface ownership, reversed depth, coverage, and GL-state restoration. |
| `MaterialCoverageGlTest` | Production coverage shaders, masked alpha, dissolve/noise coordinates, and vertex/fragment compatibility. |
| `RenderPerformanceTest.MultiDrawSeparatesApiCallsAndDraws` | Mocked accounting of API calls, constituent ranges, and triangles. |

These tests run logic without the engine or render synthetic scenes in a separate GL context. Check appearance and callback ordering in the installed game. To assess performance, compare measured CPU/GPU timings under matching scenes and collection settings. [Performance monitor cases](performance-monitor.md) cover the monitor itself.

## Automated lifecycle checks

`renderer-lifecycle-tests` compiles the concrete coordinator against recording owner operations. It verifies phase selection, ordering, nested/repeated teardown, terminal state, and generation handling without a game or GL context. Concrete capture/editor tests remain in `material-lifecycle-tests`; performance monitor tests verify GPU deletion/abandonment, stale closures, CPU history, and active-host unwind. Device predicate tests use a native-query stub in `performance-gl-tests`.

`studio-capture-gl-tests` verifies actual native identity, live deletion versus loss, shader/submission/texture recreation, and queue-first geometry release. The context fixture exposes its real native handle and can restore a previous WGL/EGL context. `scene-preview-gl-tests` verifies active target restoration and resource recreation after loss. Windows execution establishes WGL behavior; Linux SDL/GLX/EGL behavior requires its own build/run evidence. These checks do not establish installed-engine appearance or performance.
