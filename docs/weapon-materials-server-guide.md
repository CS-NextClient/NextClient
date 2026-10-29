# Applying server materials

[Russian translation](weapon-materials-server-guide.ru.md) | [API reference](weapon-materials-server-api.md) | [Authoring guide](weapon-materials-authoring.md)

The example requires AMX Mod X, ReAPI, the `NextClientServerApi` server module, and its `next_client_api` include.

## Deliver and bind a material

1. Create a preset using the [preset format](weapon-materials-server-api.md#preset-format) and store it at `materials/my_server/skins.mat`.
2. Validate it with `matgen check <preset.mat> --game <game>/cstrike`.
3. Register the preset and each custom image with `ncl_upload_file` in `plugin_precache`. Both the server and client paths for `.mat`, `.png`, and `.tga` files must be under `materials/`. Shipped collection images are already installed with the client.
4. Bind the material to the weapon, then set instance parameters using finite float values.

The function gives a Glock and sets wear to `0.65625` within `0..1`. Call it from the plugin's item-grant logic.

```pawn
#include <amxmodx>
#include <reapi>
#include <next_client_api>

new const PRESET[] = "materials/my_server/skins.mat";

public plugin_precache()
{
    ncl_upload_file(PRESET, PRESET);
}

public give_golden_glock(id)
{
    new weapon = rg_give_item(id, "weapon_glock18");
    if (weapon <= 0)
    {
        return;
    }

    ncl_mat_bind(weapon, PRESET, "golden_slide");
    ncl_mat_param_all(weapon, "uWear", 0.65625);
}
```

On a compatible client with shaders and materials enabled, the granted Glock uses `golden_slide` with wear `0.65625`. The preset texture name must match the model texture name.

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
| `r_mat_entities` | Print entity indices and model names from the next 64 rendered-entity notifications, suppressing duplicate indices. |
| `r_mat_bind` | List bindings; `(never drawn)` marks an index not requested by the renderer. |
| `r_mat_bind <entity> <preset> <material>` | Apply a local diagnostic binding. |
| `r_mat_bind <entity>` | Clear local bindings for that index. |
| `r_mat_param <entity> <name\|texture/name> <value>` | Set an instance parameter on all matching materials or one named texture. |

Synchronized-owner updates and removal preserve console bindings and other owners' material values. A binding or parameter update to a synchronized owner replaces that owner's local parameter edits with its complete synchronized values and defaults. Console textures reserve capacity before synchronized textures; at the 16-texture limit, a console command can replace an admitted texture but cannot add a new distinct one. Removing a console binding can admit a retained synchronized texture. The [Studio diagnostics](renderer-diagnostics.md) describe draw dumps and image capture.

[Dissolve, noise and occluded effects](material-effects.md)
