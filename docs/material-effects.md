# Material effects

[Russian translation](material-effects.ru.md) | [Material format](weapon-materials.md) | [Server API](weapon-materials-server-api.md)

The `studio` and `weapon_finish` materials support transparency, distortion, outlines, and highlights behind walls. Open the editor with `r_mat_panel` and select the program. The lighting tab’s test-obstacle checkbox covers the left side of the model so effects can be checked behind an obstacle. Compare shows the model without material effects and preserves the scene settings.

The `studio_effects_v1` material profile defines these effects. Parameter names, types, and allowed values must match the profile. See the [manifest description](weapon-materials.md#program-manifest) for requirements.

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

Noise hides individual areas of the surface. `uNoiseSeed` controls the pattern, and `uNoiseSpeed` controls how often it changes. Outlines and mouse selection respect transparent areas. A material can contain up to 64 parameters, including defaults.

## Geometry glitch and RGB separation

Glitch deforms the model horizontally in viewport pixels and separates the red and blue channels of the base skin. The effect uses deterministic horizontal bands, time and a seed. It applies to the selected material. Geometry, hit testing and occluded effects use the same transform; source alpha and dissolve/noise coverage remain active. A zero amount disables both components, and zero speed freezes the pattern. Coarse meshes have fewer vertices available for deformation.

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

Visible model surfaces retain their material appearance. Outline and glow follow the selected visibility mode, including with MSAA. Check partially covered edges and thin obstacles when comparing antialiasing settings.

When investigating a problem, save the output of `r_mat_scene_state` in the editor or `r_preview_state` in the game together with a screenshot. These commands report preview status and details of the latest effect rendering.

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

To make the effect available beyond the usual visibility area, explicitly enable outside-PVS transmission for the bound entity through the server material API.

Outside-PVS transmission requires an active binding and ready NextClient 2.6+ recipients. The permission follows weapon pickup and drop and ends when the binding or session ends. Ordinary clients retain their normal visibility rules. Server visibility restrictions still apply, and a recently visible entity may remain visible briefly after permission is disabled.

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
