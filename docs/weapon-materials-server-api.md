# Server material API

[Russian translation](weapon-materials-server-api.ru.md) | [Server guide](weapon-materials-server-guide.md) | [Material format](weapon-materials.md)

The `NextClientServerApi` server module provides Pawn natives for binding materials to entities. A binding identifies an entity, a preset path, and a material name within that preset. Preset and image files use the private resource download mechanism; bindings and instance parameters use entity synchronization. The client loads a preset when it first receives a binding to a material in that file. Shader materials require `r_studio_shader 1` and `r_studio_materials 1`.

## Natives

```pawn
native ncl_mat_bind(entity, const preset[], const material[]);
native ncl_mat_unbind(entity, const preset[], const material[]);
native ncl_mat_clear(entity);
native ncl_mat_through_walls(entity, bool:enabled = true);
native ncl_mat_param(entity, const texture[], const name[], Float:value);
native ncl_mat_param_all(entity, const name[], Float:value);
```

The natives return `1` on success or `0` on failure. Invalid arguments are reported in the server module log. `ncl_mat_unbind` also returns `0` when the specified binding is absent.

| Native | Contract |
|---|---|
| `ncl_mat_bind` | Bind the named preset material. Adding a new binding replaces the rendered material for the same texture. Binding an existing preset/material pair again leaves it unchanged. Stored instance parameter values remain in effect. |
| `ncl_mat_unbind` | Remove the named preset material binding. |
| `ncl_mat_clear` | Clear the entity's materials and stop its material synchronization. |
| `ncl_mat_param` | Set one instance scalar on the material identified by its model texture name. |
| `ncl_mat_param_all` | Set one instance scalar on every bound material declaring that parameter. |
| `ncl_mat_through_walls` | Explicit transmission outside PVS for a bound entity to ready NextClient 2.6+ recipients. Disable restores PAS material synchronization. See [effects](material-effects.md#server-control). |

Any entity with an edict can carry a binding. For carried Counter-Strike weapons, the server module applies the binding to the player entity. The client uses that binding for the first-person weapon and carried `p_` model. After the weapon is dropped, the binding applies to the weaponbox entity. The server module updates this mapping as ownership changes. The client applies received bindings to the rendered entity index.

Each binding targets a model texture name. All meshes using that texture receive the material. By default, the server selects recipients using the entity's potentially audible set (PAS). It handles late joins, PAS reentry, and entity serial numbers. The client replaces the affected synchronized owner's bindings and parameters. Destroying or moving an owner removes only that owner's bindings at its recorded rendered index; other owners and console bindings remain intact. When several synchronized owners target the same texture, the highest synchronization ID supplies its material.

Instance parameters are finite 32-bit floats in the parameter's units. The server stores values separately from bindings and rejects NaN and infinities before key registration or state mutation. The client applies values to matching instance scalars and clamps them to declared bounds. Fractional choice values are rejected before clamping; out-of-range integer indices clamp to the range. An invalid snapshot preserves the owner's previous state. A parameter update resets only that owner's material copies to preset defaults and synchronized effect policy, then applies the owner's complete current parameter set. Removing a parameter restores its preset default; removed occlusion and outline activations return to disabled network defaults. Parameter updates do not resolve bindings again or reconstruct other owners' materials.

| `weapon_finish` parameter | Range |
|---|---|
| `uWear` | `0..1` |
| `uLayerMix` | `0..1` |

Client and server development builds use one coordinated format. Bindings carry string-ID pairs; parameters carry key-ID/float pairs. Only parameter keys enter the string registry. User messages are reliable, ordered, and limited to 192 payload bytes. The server prepares the complete sequence before sending, chunks collections across messages, and sends clear once before repopulation. Create, late join, and visibility reentry reconstruct the full retained state. Multiple messages do not form an atomic transaction.

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

Material synchronization retains the full ordered preset/material pair set. The server derives deltas from that full set, so late join and visibility reentry receive the same bindings as existing recipients, including several materials from one preset. The client retains valid resolved candidates even when the rendered entity's 16-texture budget excludes them. Console textures reserve distinct slots first, then synchronized owners are admitted by descending synchronization ID and each owner's request order. Shared textures consume one slot. The first valid request for a texture reserves its position; the last valid assignment supplies that owner's material. Removing or moving a binding can admit a retained candidate without another update for its owner. A hidden owner's parameter updates remain available when a higher-priority owner is removed.

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

Player models use the base texture selected by the engine's `StudioSetupSkin`, including team-color remapping. Specify the original texture name, for example `DM_Base.bmp`; each entity retains its own binding. The `weapon_finish` layer set is designed for weapon finishes and can also be applied to other Studio models.

Client-side synchronization is implemented in [`MaterialSyncSystem.cpp`](../nextclient/engine_mini/src/client/ncl_entity/MaterialSyncSystem.cpp); binding storage and validation are in [`ServerMaterials.cpp`](../nextclient/engine_mini/src/graphics/render/material/ServerMaterials.cpp).
