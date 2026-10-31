# Authoring weapon materials

[Russian translation](weapon-materials-authoring.ru.md) | [Material format](weapon-materials.md) | [Server API](weapon-materials-server-api.md) | [Editor verification](testing/material-editor-scene.md)

A material specifies a shader program, parameter values, and texture references. The shipped `weapon_finish` program applies a pattern, wear, an exposed underlay, and highlights over the model's base skin. A model document is stored beside the model: `models/v_ak47.mdl` uses `models/v_ak47.mat`.

## Add wear to a base skin

1. In the disconnected main menu, run `developer 1` and open Material editor in the upper-left tools group, or run `r_mat_panel`. The preview requires OpenGL 3.3 and uses the material shader independently of gameplay shader settings.
2. Open the model catalog (`...`) on the Material tab and select a model, for example `models/v_ak47.mdl`. Choose Model view or Weapon view below the scene. Click a surface in the scene or select its texture on the Material tab.
3. Select `weapon_finish`.
4. Set `uWearMap` to a threshold map from the shipped collection. Set `uUnderlay` to an image from `materials/surfaces/` and adjust `uUnderlayTint`.
5. Adjust `uWear` and `uWearSoftness` to inspect the exposed layer and its transition.
6. Adjust `uSpecular` and `uGloss` for the scene-light highlight. Use `uSphereMap` and `uReflectivity` for a matcap reflection, sampled from an image of a lit sphere according to the surface normal and camera position. Select a reflection mask through `uReflectMask`. Use `uReflectMaskTransform` to place it and `uReflectTint` to multiply the colors of both the scene-light highlight and the matcap reflection.
7. Set the `uWear` and `uLayerMix` values to save, and save the document as `models/v_ak47.mat`.

The coating wears away in dark regions of the wear map before it wears away in light regions. A reflection mask multiplies highlight strength: white reflects fully, black suppresses it. Use masks from `materials/masks/reflect/` for this purpose. `uReflectWear` controls how highlights follow exposed areas: `1` restricts them to exposed areas, `-1` restricts them to the remaining coating, and `0` uses the reflection mask alone. Keep wear thresholds and reflection strength in separate maps when authoring both properties.

The scene's light direction and color affect specular highlights. Their intensity follows direct illumination; matcap intensity follows the total light level at the model. Inspect the material under both bright and shaded lighting.

## Use the editor

The panel builds its controls from the selected program's manifest. Materials are assigned by model texture name. Meshes using the same texture receive the same material. Skin-family selection changes the texture selected for each mesh.

The Skin and Body controls preview other model variants while the panel is open. Their values apply to the selected model during editing. Models with one skin family or body variant have the corresponding control disabled.

The editor fills the window with a native preview scene on the left and material controls on the right. In Model view, drag with the left mouse button to orbit the model, use the right or middle button to pan, and use the wheel to zoom. Frame model centers the selected model and fits its bounds. In Weapon view, dragging rotates or offsets the model and the wheel adjusts its field of view. Mirror changes the weapon presentation. Mouse capture ends when the button is released, the viewport loses focus, or the editor closes.

Controls below the viewport select the view mode, mirror, animation, frame, and playback speed. The Material tab provides the model picker and catalog. Skin and Body select model variants. In Weapon view, Reset weapon camera restores the default position, rotation, and field of view while preserving mirror, animation, material, and lighting. Animation uses the editor's own clock. Sequence entries include their indices because names can repeat.

Hovering outlines the visible parts that use the texture under the pointer. A short left click selects that texture. Moving at least four pixels starts rotation and suppresses selection. Picking selects the nearest surviving surface of the editable model and skips transparent holes. The test occluder does not block texture selection. Changing texture through the scene uses the same Save, Discard, and Cancel choices as the texture field.

The catalog uses a continuous scrollable list with a thumbnail for each row. Filtering searches the whole catalog and scrolls to the first matching model. Images load for visible rows as the list scrolls. Thumbnails show the base skin and default body parts in a rest pose. They do not include authored materials or animation and do not consume engine model-table entries. A failed thumbnail has a red marker; the model remains selectable. Rescan refreshes files and images.

The Lighting tab controls azimuth, elevation, RGB color, ambient strength, and directional strength. Neutral, Ambient lighting only, and Grazing light provide starting values. Show sun and rays displays a colored 3D orientation indicator in the viewport. Its arrows follow light travel; its position stays readable while the camera pans or zooms. The checkbox changes only visibility.

Animate sun rotates the light around the model at a fixed elevation above the horizon. An initial elevation outside 5..80 degrees changes to 35 degrees when rotation starts; elevation remains editable within 5..80 during rotation. Sun speed is independent of model playback and ranges from 0.25x to 4x. One revolution takes 24 seconds at 1x, 96 seconds at 0.25x, and six seconds at 4x. Speed changes take effect during rotation without restarting it. It works while model playback is paused and while the sun indicator is hidden. Turning it off holds the current direction. Angle controls follow the moving light; color, strength, material, and model playback remain independent. Camera, lighting, animation, and comparison changes preserve the working material.

The model catalog (`...`) lists installed `.mdl` files under `models` across game search paths, including download directories. It scans `models` and three nested directory levels and collects at most 16,384 paths before deduplication. The filter matches a case-insensitive substring of the full path. The counters show matching and total models. Refresh rescans the catalog; Remove clears the model. Texture and sequence companion files are excluded using file-header checks. A selected model is validated and prepared before its metadata and textures become available. A failed selection reports an error and retains the current model and working material.

Preview loading shares a process budget of 128 new engine model-table entries across the editor and settings preview. A separate texture companion consumes an entry too. Closing the editor or changing video mode retains this accounting. An engine model-table reset clears it. Reaching the limit reports an error; models already in the table remain usable.

Material changes become dirty until saved or restored to their saved values. Closing the workspace or changing the model or texture offers Save, Discard, and Cancel. Cancel and a failed save preserve the working material. Compare switches authored materials off only for this viewport. Gameplay shader and material cvars retain their values.

Drag the label beside a numeric field horizontally to change its value. Dragging approximately 220 pixels covers the parameter's full declared range; Shift reduces the rate tenfold, and Ctrl increases it tenfold. Vector components have separate handles. Typed and dragged values follow manifest bounds. Fields without declared bounds accept finite values. Ordinary scalars retain float precision independently of slider and drag increments. Nonfinite input preserves the last valid value.

Choice parameters accept only integer indices; fractional indices are rejected before clamping.

A color parameter opens a picker with RGB and alpha values in `0..255`, hue, saturation, brightness, and transparency controls. Changes preview immediately; Cancel restores the starting color, and Default restores the program's default.

The texture browser shows folders, thumbnails, and an enlarged preview for the shipped collection. Selecting a row applies the image to the model. Select accepts the image, Back restores each selected texture's prior reference, and Remove clears it. The edit session persists while browsing the same target. A texture path can also be entered directly. The editor checks the path syntax. Before delivery, validate the referenced image files with `matgen check`.

## Edit a texture group

Click Group beside Texture, select the model textures to edit, and press Apply. Select all marks every texture. A group contains 1..64 texture names. Changing a parameter sets that entire parameter on every selected texture; other parameters retain each texture's values. Changing the program affects the whole group, preserves parameters with a matching name and type, and uses defaults for other parameters.

An asterisk and a tooltip identify mixed parameter values. Numeric, vector, and image fields show the first texture's value; named-choice fields show Mixed values. Save writes every selected texture and any checked related-model destinations. Changing the group, model, or texture, or closing the editor, uses Save, Discard, and Cancel when the material is dirty.

The image browser and color picker preserve the original parameter separately for every selected texture. Back, Cancel, or closing the preview restores those values, including mixed values and parameters that were absent. Other working changes remain intact. Accepting applies the chosen value to the whole group.

## Add a pattern

1. Run `matgen uv models/v_ak47.mdl` to export a wireframe PNG for each model texture.
2. Paint on the exported UV template in an image editor. Pattern alpha controls which areas show the base skin.
3. Save the image under the game directory, set `uPattern`, and adjust `uLayerMix`, `uPatternTint`, and `uPatternTransform`.
4. Configure the wear map and underlay, then inspect the material over the full wear range.

## Bake model-specific wear

```text
matgen bake edges models/v_ak47.mdl
```

The command writes a grayscale PNG for each texture used by the selected skin family. Convex edges are dark and wear first; recessed regions are light. The bake extends image contents beyond UV island edges to reduce filtering seams. Set the result as `uWearMap`.

External baking tools can use the model's existing UV layout after importing its SMD data. A curvature bake can supply `uWearMap`; ambient occlusion can supply it when wear should start in recesses. Painted artwork supplies `uPattern`, and the exposed surface image supplies `uUnderlay`. Configure an island margin during baking.

## Use shipped images

The [shipped collection](tools/material-masks.md) contains wear, noise, scratch, dirt, and reflection masks, underlay surfaces, and matcaps. Reference them relative to the game directory, for example `materials/masks/wear/even.png`. These files are installed with the client. Their references still count toward the document or preset limit of 64 distinct images. Existing collection paths and contents remain stable; additions use new names.

Custom images must be PNG or TGA with at most 1,048,576 pixels. The limit applies to area, so rectangular images can have a side longer than 1024 pixels. See the [format reference](weapon-materials.md) for path and file budgets.

## Save related models and deliver a preset

The `v_`, `p_`, and `w_` models have separate documents. The Save targets checkboxes select individual sibling models and start unchecked. Save prepares each selected destination and applies the material to its matching texture. Missing textures, preparation failures, and write failures are reported per path. Any failed destination keeps the session dirty. Inspect each related model in the preview before delivery.

1. Validate each document with `matgen check models/v_ak47.mat --game <game>/cstrike`. Image paths resolve against `--game`, or the current directory when omitted.
2. Use the panel's preset-material export to print an entry for a server preset.
3. Place the entry in the server's preset and distribute the preset with its custom images. References to the shipped collection use the images already installed with the client.

Outside an edit session, server entity bindings take precedence over model documents. During editing, the working material temporarily overrides that selection. The editor's source label identifies the underlying material source. Use the [server guide](weapon-materials-server-guide.md) to inspect bindings and instance parameters.

## GameUI dependency ownership

CGameUI resolves the editor, scene preview, renderer, filesystem, and VGUI providers at initialization and supplies them to the dialog and its immediate children. The material dialog owns its filesystem resource source, catalog, edit session, and preview state. Developer controls are composed during Start, when the real engine callback table is available; video controls receive the resolved renderer and their shared configuration provider through the options dialog.

Image panels and browser entries retain the exact shared image cache until VGUI actually deletes them. Loading, drawing, and texture deletion use the supplied surface instance. Paint reads the cache and providers directly. Terminal closure rejects posted edits, ends parameter previews and the working session, and releases scene authoring resources before notifying CGameUI. Shutdown blocks new authoring opens and dispatches one final VGUI frame before disconnecting providers, so delayed panel deletion can release its textures.

## Inspection

| Command | Action |
|---|---|
| `r_mat_panel` | Open the editor from the disconnected main menu. |
| `r_mat_edit` | Use console editing through `models`, `textures`, `begin`, `set`, and `save`. |
| `r_mat_scene_state` | Print the editor target, camera, light, animation, and engine preview diagnostics. |
| `r_preview_state` | Print studio readiness, device generation, model-entry accounting, and the last loading error. |
| `r_studio_materials 0` | Disable authored materials in gameplay. Use Compare in the editor. |
| `r_studio_dump` | Record the selected backend and material inputs for a scene. |

[Material effects: dissolve, noise, glitch, fill, and outline](material-effects.md)
