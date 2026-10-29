# Material effects

[Russian translation](material-effects.ru.md) | [Material format](weapon-materials.md) | [Server API](weapon-materials-server-api.md)

Authored `studio` and `weapon_finish` materials support the same coverage and occluded effects. Select either program in Material Editor. The Lighting tab contains **Show test occluder**, which places a depth-writing panel over the left part of the viewport. Compare disables authored effects while preserving the scene controls. Open the editor from the developer menu or with `r_mat_panel`.

Shared behavior is declared by the embedded `studio_effects_v1` render contract. Semantic roles and accepted declarations are defined by the renderer; ordinary manifest parameters carry no effect capability. The [material format](weapon-materials.md#program-manifest) describes enrollment and defaults.

## Dissolve and noise transparency

| Parameter | Meaning |
|---|---|
| `uDissolveMap` | Dissolve threshold in the image's red channel. Empty uses white. |
| `uDissolveTransform` | `[scale_x, scale_y, offset_x, offset_y]`. |
| `uDissolveSpace` | `0`: surface UV; `1`: viewport coordinates. |
| `uDissolve` | `0..1`. Zero keeps every fragment; one removes every fragment. Intermediate values discard mask values at or below the amount. |
| `uNoise` | `0..1` procedural transparency. Zero keeps every fragment; one removes every fragment. |
| `uNoiseSpace` | `0`: surface UV; `1`: viewport coordinates. |
| `uNoiseScale` | Noise cells per coordinate unit, `1..1024`, default `256`. |
| `uNoiseSeed` | Reproducible pattern selection, `0..32767`. |
| `uNoiseSpeed` | Pattern updates per second, `0..60`. Zero freezes the pattern. |

Viewport coordinates start at the lower-left corner of the rendered view and divide both axes by its height. One screen unit therefore spans the viewport height, and noise cells remain square at any aspect ratio. The pattern stays fixed in the view while the model moves. UV coordinates follow the model. Dissolve and noise choose their coordinate spaces independently. Only fragments that survive both tests remain visible. Masked base-skin holes remain holes.

Noise changes coverage rather than adding grain to the surface color. Its seed and captured render time determine the pattern. The surface, occluded effects and mouse picking use the same coverage. Material documents support up to 64 parameters, including values filled from manifest defaults.

## Geometry glitch and RGB separation

Glitch deforms the model horizontally in viewport pixels and separates the red and blue channels of the base skin. The effect uses deterministic horizontal bands, the captured rendering clock and a seed. It applies to the selected material. Geometry, hit testing and occluded effects use the same transform; source alpha and dissolve/noise coverage remain active. A zero amount disables both components, and zero speed freezes the pattern. Coarse meshes have fewer vertices available for deformation.

| Parameter | Range | Default |
|---|---|---|
| `uGlitchAmount` | `0..1` | `0` |
| `uGlitchDisplacement` | `0..64` pixels | `12` |
| `uGlitchBands` | `1..128` bands across viewport height | `24` |
| `uGlitchRgbSplit` | `0..16` pixels | `2` |
| `uGlitchSpeed` | `0..60` pattern updates per second | `12` |
| `uGlitchSeed` | `0..32767` | `0` |

The [KinoGlitch examples](https://github.com/keijiro/KinoGlitch) illustrate line jitter, color drift and block corruption. The [3D model glitch example](https://github.com/callumSharman/glitch-shader) combines geometry deformation and RGB separation. These are visual references for this implementation.

## Editing a texture group

Open **Group** beside the texture field, check the required textures or use **Select all**, then choose **Apply**. The displayed parameter values come from the first selected texture. An asterisk marks a parameter with different values in the group. Editing that parameter applies its value to every selected texture and preserves all other parameters. Groups with different shader programs show **Mixed shaders**; choose a common program before changing its parameters.

Save writes all selected entries together and preserves unselected entries in the model document. Related models receive the group only when all selected texture names exist in that model. Compare covers the whole group. Selecting a single texture returns to single-texture editing through the normal unsaved-change confirmation.

## Fill, outline and glow

| Parameter | Meaning |
|---|---|
| `uOccludedAmount` | Fill intensity, `0..1`, default `0`. |
| `uOccludedColor` | Fill RGBA. |
| `uOutlineMode` | `0`: occluded silhouette only (default); `1`: entire silhouette; `2`: visible silhouette only. Glow follows the same mode. |
| `uOutlineWidth` | External outline width in pixels, `0..16`, default `0`. Zero also disables glow. |
| `uOutlineColor` | Outline and glow RGBA. |
| `uGlowRadius` | Additional soft halo radius in pixels, `0..64`, default `8`. |
| `uGlowStrength` | Halo intensity, `0..4`, default `0`. |

Select **Outline mode** in the Material tab: **Occluded only** draws the hidden external silhouette, and **Always** draws the complete external silhouette with or without an obstacle. **Visible only** draws only the visible part of the external silhouette. The mode applies to outline and glow. Fill remains restricted to hidden pixels, and the line where a wall crosses the model does not become an outline. Dissolve, noise and masked skin alpha apply in all three modes.

The renderer compares the model's nearest surface with the completed scene depth. With MSAA, both depth buffers retain the same sample count, and corresponding covered samples are compared individually. A pixel is hidden only when all its covered model samples are hidden. Visible surfaces retain their authored appearance, and the outer halo excludes pixels covered by the model's MSAA samples.

The renderer captures masks while the original skin and pose are available. Each selected model includes its ordinary meshes as self-occluders. Server bindings and local materials on attached player weapons identify affected entities before the parent model draws. Mask targets are cropped to conservative bone bounds, expanded by the effect radius and maximum glitch displacement. Boundary propagation and composition run as full-screen passes over the viewport, with bounds checks for the stored mask. Near-plane intersections use the full view. The cache retains at most 512K mask pixels between views. `r_mat_scene_state` includes counters for the last effect view: entities, captured packets, mask pixels, reused targets, depth format and sample count. `r_preview_state` reports the same effect counters in a loaded game.

Material Editor renders its authoring scene, material effects, picking and sun gizmo in one offscreen framebuffer. It then copies color and depth into the caller viewport, restricted by its clip. The scene target preserves the caller color/depth formats and sample count. Scene geometry and mask depth use the same offscreen rasterization path.

## Server control

In a network game, only explicit server calls to `ncl_mat_param_all` or `ncl_mat_param` set `uOccludedAmount` and `uOutlineWidth`. A synchronized binding starts both values at zero regardless of its preset; removing a server parameter disables that effect again. Local model documents and console bindings cannot activate fill or outline. Material mutation commands require a disconnected client. The main-menu editor supports all effects. `ncl_mat_through_walls` separately permits transmission outside PVS.

Create a preset with the required model texture name, deliver it with the existing private download API, and bind it through `ncl_mat_bind`. The following material parameters enable orange fill and outline:

```json
{
    "program": "studio",
    "params": {
        "uOccludedAmount": 0.5,
        "uOccludedColor": [1.0, 0.25, 0.05, 0.7],
        "uOutlineWidth": 2.0,
        "uOutlineColor": [1.0, 0.5, 0.1, 1.0],
        "uGlowRadius": 8.0,
        "uGlowStrength": 1.0
    }
}
```

Explicitly enable transmission outside PVS, the potentially visible set, after binding the material:

```pawn
ncl_mat_bind(entity, "materials/presets/effects.mat", "highlight");
ncl_mat_through_walls(entity, true);
ncl_mat_param_all(entity, "uOccludedAmount", 0.5);
ncl_mat_param_all(entity, "uOutlineWidth", 2.0);
ncl_mat_param_all(entity, "uOutlineMode", 2.0);
ncl_mat_param_all(entity, "uGlowStrength", 1.0);
ncl_mat_param_all(entity, "uDissolve", 0.25);
ncl_mat_param_all(entity, "uNoise", 0.5);
ncl_mat_param_all(entity, "uNoiseSpeed", 4.0);
```

Enabling transmission requires an active material binding. It applies to ready NextClient 2.6+ recipients and follows the material's rendered owner as a weapon is picked up or dropped. Enabled material synchronization is broadcast; disabling returns it to PAS. Clearing, final unbind, source reuse, owner reuse and map replacement revoke the permission. Ordinary clients retain normal visibility rules. GameDLL rules such as no-draw, missing models, spectators and groups remain authoritative; packet entity capacity still limits transmission. The normal GameDLL visibility cache can retain recently visible entities briefly after disabling. The implementation preserves the [GameDLL's state construction and rule checks](https://github.com/rehlds/ReGameDLL_CS/blob/master/regamedll/dlls/client.cpp).

| Instance scalar | Value range |
|---|---|
| `uDissolve`, `uNoise`, `uOccludedAmount` | `0..1` |
| `uNoiseSeed` | `0..32767` |
| `uNoiseSpeed` | `0..60` updates per second |
| `uOutlineWidth` | `0..16` pixels |
| `uOutlineMode` | Integer indices `0`, `1`, `2` |
| `uGlowStrength` | `0..4` |

Coordinate spaces, mask transforms, colors, noise density and halo radius are preset parameters. Instance values are finite floats in the listed units. Ordinary scalars retain float precision. `uOutlineMode` requires an integer index, such as `2.0` in Pawn. Fractional indices are rejected before clamping. A server installation requires the updated module and Pawn include; client DLL installation alone cannot enable transmission outside PVS.

The test plugin, resources and verification commands are in [test_support/plugins/nextclient_materials_test](testing/materials-test-plugin.md).
