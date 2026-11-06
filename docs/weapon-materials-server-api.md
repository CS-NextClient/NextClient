# Server material API

[Russian translation](weapon-materials-server-api.ru.md) | [Server guide](weapon-materials-server-guide.md) | [Material format](weapon-materials.md)

The `NextClientServerApi` module lets AMX Mod X plugins assign materials to game entities. An assignment requires an entity, a preset path, and a material name from that preset. The Pawn functions available to plugins are called natives and are listed below. Preset and image files reach the client through private resource downloads. The server separately sends material assignments and parameter values for each entity. Displaying materials requires `r_studio_shader 1` and `r_studio_materials 1`.

## Natives

The natives return `1` on success or `0` on failure. Invalid arguments are reported in the server module log. `ncl_mat_unbind` also returns `0` when the specified binding is absent.

| Function | Purpose |
|---|---|
| `ncl_mat_bind` | Bind the named preset material. Adding a new binding replaces the rendered material for the same texture. Binding an existing preset/material pair again leaves it unchanged. Stored instance parameter values remain in effect. |
| `ncl_mat_unbind` | Remove the named preset material binding. |
| `ncl_mat_clear` | Clear the entity's materials and stop its material synchronization. |
| `ncl_mat_param` | Set one instance scalar on the material identified by its model texture name. |
| `ncl_mat_param_all` | Set one instance scalar on every bound material declaring that parameter. |
| `ncl_mat_through_walls` | Explicit transmission outside PVS for a bound entity to ready NextClient 2.6+ recipients. Disable restores PAS material synchronization. See [effects](material-effects.md#server-control). |

Materials can be assigned to server entities. For Counter-Strike weapons, the assignment follows pickup and dropping. While a player carries the weapon, the material appears on the first-person weapon and carried `p_` model. After dropping, it appears on the weapon on the ground.

A material applies to every model part using the specified texture name. The client receives current assignments when joining and when the server resumes sending it data about the entity. Removing one material preserves the others. When several assignments affect the same texture, inspect the selected material with the [diagnostic commands](weapon-materials-server-guide.md#diagnose-a-missing-material).

Instance parameters are set separately for each entity: two weapons using the same material can have different wear values. Values must be finite numbers in the parameter’s units and range. Choice parameters accept only integer indices. Invalid values leave the previous setting unchanged. Removing a parameter restores its preset value; removing a server fill or outline parameter disables that effect.

| `weapon_finish` parameter | Range |
|---|---|
| `uWear` | `0..1` |
| `uLayerMix` | `0..1` |

Use matching client and server-module versions. Joining players and players returning to the update area must receive the current material assignments. Verify the result in the game after changing either side.

## Preset format

```json
{
    "version": 1,
    "materials": [
        {
            "name": "golden_slide",
            "texture": "glock_slide.bmp",
            "material": {
                "program": "weapon_finish",
                "params": {
                    "uWearMap": { "texture": "materials/masks/wear/blotches.png" },
                    "uWearMapTransform": [8.0, 8.0, 0.0, 0.0],
                    "uUnderlay": { "texture": "materials/surfaces/brass.png" },
                    "uUnderlayTint": [1.0, 0.55, 0.1, 1.0],
                    "uWear": 0.5
                }
            }
        }
    ]
}
```

`name` is unique within the file and matches without regard to case. `texture` is the model's texture name. `material` has the same program and parameter structure as a model document. Preset parameter values initialize a binding; instance values override them.

An entity can display materials on at most 16 distinct textures. If that limit or overlapping assignments hide an expected material, inspect and simplify the active bindings before comparing the image.

## Client validation and limits

| Resource | Limit |
|---|---|
| Preset version | `1` |
| Materials per preset | 128 |
| Distinct image files referenced by a preset | 64, including shipped images |
| Image-reference path | 96 bytes |
| Preset file path | 128 bytes, relative path ending in `.mat` |
| Preset file size | 1 MiB |
| Cached presets | 32, including failed load records, until cache clearing |
| Distinct admitted texture bindings per rendered entity | 16 |

Invalid structure, duplicate names, invalid image paths, and exceeded file budgets reject the whole preset. Image paths must meet the [material path rules](weapon-materials.md#file-and-image-limits). The client discards individual materials that reference an unknown program or fail manifest validation. Valid entries remain available. Image loading enforces PNG/TGA data and the 1,048,576-pixel area limit.

For player models, use the original texture name, for example `DM_Base.bmp`. Team-color remapping remains available. The `weapon_finish` program can also be applied to Studio models other than weapons.
