# Weapon material format

[Russian translation](weapon-materials.ru.md) | [Authoring guide](weapon-materials-authoring.md) | [Server API](weapon-materials-server-api.md)

When drawing a Studio model, the client selects a material by texture name. Outside an edit session, it checks the entity's server binding first, then the model's `.mat` document. If neither source supplies a material, the client uses the built-in material based on the texture flags. An active edit session temporarily overrides the selected material for its target. Materials require `r_studio_shader 1` and `r_studio_materials 1`.

## Model document

The document beside `models/v_glock18.mdl` is `models/v_glock18.mat`. Its `materials` object maps model texture names to materials. Texture-name matching is case-insensitive. Every mesh using a selected texture receives its material, including meshes in other body parts. The selected skin family determines each mesh's texture before the client selects its material.

```json
{
    "version": 1,
    "materials": {
        "glock_slide.bmp": {
            "program": "weapon_finish",
            "params": {
                "uWearMap": { "texture": "materials/masks/wear/blotches.png" },
                "uUnderlay": { "texture": "materials/surfaces/brass.png" },
                "uWear": 0.5
            }
        }
    }
}
```

A material specifies a required `program` and an optional `params` object. Parameter names match the program manifest exactly, including case. A number is a scalar, an array of four numbers is a vector, and `{"texture": "path"}` is an image reference. The client checks parameter types and ranges against the manifest. Scalars and every vector component must be finite and within declared bounds. Omitted parameters receive declared defaults.

## File and image limits

| Resource | Limit |
|---|---|
| Document version | `1` |
| Texture entries per model document | 64 |
| Parameters per material | 64 |
| Distinct image files referenced by a document | 64 |
| Image path in a model document | 128 bytes |
| Image path in a server preset | 96 bytes |
| Image formats | PNG or TGA |
| Image area | 1,048,576 pixels |

Image paths are relative to the game directory. Use forward slashes and the `.png` or `.tga` extension. Absolute paths, drive prefixes, backslashes, control characters, and empty, `.` or `..` segments are rejected. Image area is `width * height`. Shipped images count toward the file limit. Repeated references to one file count once.

## Program manifest

A program manifest declares `version`, `program`, and `parameters`. Each parameter declares its type (`scalar`, `vec4`, or `texture`) and may declare a default, bounds, label, hint, or editor widget. A scalar with `instance: true` supports per-entity writes through the [server API](weapon-materials-server-api.md#natives). The server sends a finite `Float:value` in the parameter's units. The client preserves float precision and clamps runtime writes to declared bounds. An ordinary scalar requires no step or value grid.

The optional `render_contract` field is an owned, nonempty string of at most 64 bytes. It remains an extension of manifest version `1`. The material package validates its representation only. An absent field leaves the program parameters generic. The engine publishes a manifest only when its program name matches the requested embedded program, both shader sources are embedded, and the renderer accepts its contract.

The embedded `studio` and `weapon_finish` manifests declare `"render_contract": "studio_effects_v1"`. `render::StudioProgramContract` fixes coverage, base-tint alpha, outline, occlusion, and glow roles, including their types, ranges, defaults, instance declarations. Labels, groups, and editor presentation stay in each manifest. The two restricted activation roles, `uOccludedAmount` and `uOutlineWidth`, may declare any finite default within their fixed range; other semantic defaults must match the profile exactly. Adding another embedded program with this profile requires its shader pair, embedded-resource enrollment, and a compatible manifest declaration. Coverage, picking, effects, and server policy use the profile without additional program-name registration.

The schema allows 64 parameters and eight texture parameters. Optional `groups` determine editor sections. Optional `strings` names the localization file. The shipped [`weapon_finish` manifest](../nextclient/engine_mini/assets/shaders/weapon_finish.json) contains the authoritative defaults, bounds, and editor grouping.

A scalar with `widget: "choice"` declares `choices` as an array of localized labels. Values are integer indices: `min: 0`, `max: choices.size()-1`, and an integer `default` in that range. The editor displays a non-editable list. Fractional indices are rejected before clamping; out-of-range integer writes clamp to the range. For example, `uOutlineMode` uses index `0` for occluded-only outline, `1` for the entire silhouette, and `2` for visible-only outline.

## `weapon_finish` inputs

| Parameter | Meaning |
|---|---|
| `uBaseTint` | Base-skin color multiplier. |
| `uPattern`, `uPatternTint`, `uPatternTransform` | Pattern image, tint, and UV placement. Pattern alpha also controls mixing. |
| `uLayerMix` | Pattern fraction, `0..1`. |
| `uWearMap`, `uWearMapTransform` | Threshold map and UV placement. Dark regions wear first. |
| `uWear` | Wear amount, `0..1`. |
| `uWearSoftness` | Half-width of the transition around the wear threshold. |
| `uUnderlay`, `uUnderlayTint`, `uUnderlayTransform` | Exposed surface image, tint, and UV placement. |
| `uReflectMask`, `uReflectMaskTransform` | Reflection-strength multiplier and UV placement. |
| `uReflectTint` | Color multiplier for the scene-light highlight and matcap reflection. |
| `uReflectWear` | Highlight dependence on wear, from `-1` (coating) to `1` (exposed areas), with `0` using the mask alone. |
| `uSphereMap`, `uReflectivity` | Image of a lit sphere (matcap) and reflection intensity. |
| `uSpecular`, `uGloss` | Scene-light highlight intensity and exponent. |

UV transforms contain `[scale_u, scale_v, offset_u, offset_v]`. The shader blends the pattern into the base, applies the wear threshold with `smoothstep`, mixes in the underlay, then adds masked highlights to the lit surface. Specular uses direct lighting; matcap reflection uses the total light level.

The parser and validation contracts are implemented in [`ncl_material`](../nextclient/packages/ncl_material/include/ncl_material/MaterialDoc.h). The [renderer reference](renderer-architecture.md) describes texture binding, GL resources, and draw ordering.

[Material effects](material-effects.md)
