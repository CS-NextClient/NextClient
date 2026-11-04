# Applying server materials

[Russian translation](weapon-materials-server-guide.ru.md) | [API reference](weapon-materials-server-api.md) | [Authoring guide](weapon-materials-authoring.md)

Server material delivery requires AMX Mod X and the NextClientServerApi server module. Use the [API reference](weapon-materials-server-api.md) for the supported operations.

## Deliver and bind a material

1. Create a preset using the [preset format](weapon-materials-server-api.md#preset-format) and store it at `materials/my_server/skins.mat`.
2. Validate it with `matgen check <preset.mat> --game <game>/cstrike`.
3. Register the preset and each custom image with `ncl_upload_file` in `plugin_precache`. Both the server and client paths for `.mat`, `.png`, and `.tga` files must be under `materials/`. Shipped collection images are already installed with the client.
4. Bind the material to the weapon, then set instance parameters using finite float values.

On a compatible client with shaders and materials enabled, the assigned material appears on the matching model texture. The preset texture name must match the model texture name.

## Diagnose a missing material

1. Check that the installed client supports this material API and has `r_studio_shader 1` and `r_studio_materials 1`.
2. Check that the preset arrived at the exact relative path passed to `ncl_mat_bind`.
3. Check the preset material name and the model's texture name. `matgen uv <model.mdl>` lists model textures.
4. Inspect the server module log for native argument errors. Run `matgen check <preset.mat> --game <game>/cstrike` for format, manifest, path, and image errors.
5. Inspect the loaded client's bindings with the commands below.

Changes through `r_mat_bind`, `r_mat_param` and `r_mat_presets` arguments require a disconnected client. In a network game, explicit server instance parameters `uOccludedAmount` and `uOutlineWidth` activate fill and outline, as described in [material effects](material-effects.md).

| Command | Action |
|---|---|
| `r_mat_presets [file\|clear]` | List cached presets, load a preset, or clear cached presets and all client-side material bindings. |
| `r_mat_entities` | Print entity indices and model names from the next 64 rendered entities, suppressing duplicate indices. |
| `r_mat_bind` | List bindings; `(never drawn)` marks an index not requested by the renderer. |
| `r_mat_bind <entity> <preset> <material>` | Apply a local diagnostic binding. |
| `r_mat_bind <entity>` | Clear local bindings for that index. |
| `r_mat_param <entity> <name\|texture/name> <value>` | Set an instance parameter on all matching materials or one named texture. |

Console commands are intended for local checks. Their assignments take priority over server assignments for the same texture. A server parameter update can replace local changes. One entity can use materials for at most 16 distinct textures at once; remove unnecessary local bindings when this limit is reached. Use images and dumps to check which material is displayed; see [Studio diagnostics](renderer-diagnostics.md).

[Dissolve, noise and occluded effects](material-effects.md)
