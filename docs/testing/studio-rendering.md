# Studio rendering verification

[Russian translation](studio-rendering.ru.md) | [Content preparation](../tools/studio-content.md) | [Capture procedures](../renderer-diagnostics.md)

The developer performs live-engine checks in the installed game. Record the source revision, installed DLLs, scene or demo, graphics settings, material settings, and monitor mode for each observation. Build artifacts and tests in a separate GL context provide separate evidence.

Prepare the changed libraries, the modules that use them, and the required resources from one revision with matching build settings. Verify copied hashes using the [installation and factory procedure](installation-and-factory.md).

## Runtime cases

Use `studio_test` for the flag, shell, remap, lighting, and repeated-model cases. To check drawing order for other combinations of objects, use a map or demo that shows them together.

| Case | Expected result |
|---|---|
| Texture flags | Plain, flatshade, chrome, chrome-flat, masked, and additive meshes retain their intended lighting, alpha, and depth behavior. Account for the fullbright difference below. |
| Glow shells | The base pass draws before the shell. Shells retain the configured extrusion and distinct red/blue colors. The shell must not change the next object's color. |
| Entity and scene light | Models in the dark alcove respond to available lighting. Use `r_studio_dump <name> lit` to wait for an entity-light sample. |
| Carried weapons | First-person weapons and weapons held by other players retain their appearance, pose, and attachment points. |
| Remap skin | Engine and shader paths select the same remapped base texture for the chosen `colormap`. |
| Opaque ordering | Studio/brush/Studio order remains correct with `r_studio_queue 0` and `1`. |
| Transparent ordering | Compare overlapping transparent and additive Studio models with `r_studio_queue 0` and `1`, including sprites and other model types. Object order, colors, and animation must remain correct. Replay the same demo segment three times per build with matching settings and Tracy disconnected. |
| Transparency | Transparent objects, masked/additive meshes, and first-person weapons retain their appearance and do not affect the colors or visibility of neighboring objects. |
| Materials | With the editor closed, the client selects a server binding, then a model document, then a built-in material. An active edit session temporarily overrides the selected material for its target. Values for different entities remain independent. |
| Fog | Compare engine and shader paths with the camera submerged in water. Fog must affect the models and the surrounding map consistently. Record a mismatch separately from lighting and fullbright differences. |
| Material effects | Check the three outline modes, hidden fill, glow, dissolve, noise, and glitch using the [material scene cases](material-editor-scene.md#material-effect-checks). Repeat in windowed and fullscreen modes with MSAA Off and 4x. |
| Repeated models | Use the [comparison procedure](studio-rigid-instancing.md) with matching scenes and settings. Poses, materials, and object order must remain unchanged. Compare rendering counters and measured CPU/GPU time. |

## Image interpretation

Capture a stationary scene with `r_studio_ab flags`. From the repository root, compare the resulting files, replacing `<gamedir>` with the game directory containing the captures:

```text
python tools/radar_tests/imgdiff.py "<gamedir>/flags_off.tga" "<gamedir>/flags_on.tga" --clusters --out flags_diff.png
```

The shader path honors `STUDIO_NF_FULLBRIGHT` and skips the stock planar shadow for accepted models. Those regions can differ by design. Separate them from other differences before evaluating the result.

Inspect both the overall differing-pixel fraction and the `--clusters` report. Dense regions whose brightness changes mainly in one direction require investigation even when the overall fraction passes. Small differences along silhouettes can arise from CPU/GPU arithmetic precision. Movement, animation, lighting changes, and different hardware or resolution also affect the comparison. Preserve images and diagnostic dumps with the result.

`r_studio_dump` shows the model path, the name stored in its file, the renderer used, flags, and material parameters. The name inside the file can differ from its path. If the stock engine drew the model, the dump explains why NextClient did not render it.

## Automated checks

Build the affected targets in an x86 test configuration, then run their registered CTest cases.

| Targets | Purpose |
|---|---|
| `studio-capture-gl-tests` | Model appearance, materials, effects, grouping, and rendering after resource changes. |
| `studio-pose-tests`, `studio-effect-bounds-tests` | Pose consistency and the screen area affected by effects. |
| `studio-phase-capture-tests` | Capture ranges, completeness, and exported results. |
| `renderer-lifecycle-tests`, `material-lifecycle-tests` | Reset and shutdown behavior. |
| `scene-preview-gl-tests` | Preview appearance and recovery after graphics changes. |

GL tests need a suitable context and must run without skips to establish rendering coverage. Run platform-specific checks on each affected platform. These tests complement the developer's observations in the installed game; appearance and performance still require the fixed-scene comparisons above.
