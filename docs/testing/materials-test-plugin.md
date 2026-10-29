# NextClient material effects test plugin

[Russian instructions](materials-test-plugin.ru.md)

This standalone AMX Mod X plugin creates one test model per player. It exercises authored studio and weapon_finish materials, occluded fill, occluded-only, full-silhouette, or visible-only outline/glow, dissolve, noise transparency, geometry glitch/RGB separation and explicit transmission outside PVS. It requires AMX Mod X 1.9+, the Fakemeta and JSON modules, an updated NextClientServerApi module and a ready NextClient 2.6+ client with `r_studio_shader 1` and `r_studio_materials 1`. It uses stock leet, AK47 and chicken models.

## Install

This plugin runs separately from `nextclient_testlab`. It requires an updated NextClientServerApi module with `ncl_mat_through_walls`; standard TestLab scenarios do not require that module.

1. Compile `nextclient_materials_test.sma` as described below or use `nextclient_materials_test.amxx` from the prepared package.
2. Copy the `.amxx` into the server's game directory at `addons/amxmodx/plugins/nextclient_materials_test.amxx`.
3. Copy the contents of `test_support/plugins/nextclient_materials_test/resources/` into the server's game directory, preserving `materials/test_nextclient/material_effects.mat` and `materials/test_nextclient/effects/dissolve.png`. The plugin registers both files for private resource downloads.
4. Add `nextclient_materials_test.amxx` to `addons/amxmodx/configs/plugins.ini` and change the map.

## In-game menu

Open the menu with `ncl_fx` or `ncl_fx_menu` in the client console, or type `/ncl_fx` in public or team chat. No administrator flag is required. Every action affects only the player's own target.

The main menu reports API readiness and whether the target's material binding completed. It creates or replaces a leet, AK47 or chicken target, switches between `studio` and `weapon_finish`, cycles through the three outline modes, toggles outside-PVS transmission, opens effect and distance controls, prints status, and removes the target. Creation resets the effects to their documented defaults. Program changes retain scalar values. Material actions require a ready NextClient API; status and removal remain available without readiness.

`Edit effect parameters` includes every parameter in the table below. Select a parameter to decrease or increase it, cycle the step size, set its minimum or maximum, or choose `Enter a number`. Numeric input uses the standard message input and accepts `cancel` without applying a value. Reopening the menu or running a console subcommand cancels pending numeric input. Ordinary scalars are stored and sent as finite floats in the listed units. Menu steps control button increments only. Direct numeric input retains float precision. Outline mode accepts only `0`, `1`, or `2`.

| Parameter | Normal step | Fine step | Coarse step |
|---|---|---|---|
| `dissolve`, `noise`, `fill`, `glitch` | `0.125` | `1/256` | `0.25` |
| `outline` | `0.5` pixels | `0.25` pixels | `2` pixels |
| `glow` | `0.25` | `1/64` | `1` |
| `glitch_speed`, `noise_speed` | `1` | `0.25` | `10` |
| `glitch_seed`, `noise_seed` | `1` | `1` | `100` |
| `outline_mode` | `1` | `1` | `1` |

`Move target along view` places the model 64, 160, 256, 512, 1024, 2048, 4096 or 8192 units from the current camera. Placement follows the viewing ray and can cross a wall. Use `ncl_fx move distance` for another distance in the supported range. Submenus have a back action; exiting a menu releases it. Disconnect and plugin shutdown release menus and pending input as well as the target.

## Commands

Run target commands from a player's client console. Each player controls their own test entity. The target is nonsolid and remains at its placed position. Disconnect, replacement and plugin shutdown remove it and clear its material binding.

| Command | Action |
|---|---|
| ncl_fx / ncl_fx_menu / say /ncl_fx / say_team /ncl_fx | Open the in-game menu. |
| ncl_fx spawn [leet\|ak47\|chick] | Create or replace the player's target 160 units ahead. Default model: leet. fill 0.5, outline 2, glow 1, occluded-only outline mode, outside-PVS transmission enabled. |
| ncl_fx move [distance] | Place the target ahead of the current camera, 16..8192 units. Placement follows the viewing ray and can cross a wall. |
| ncl_fx program studio | Use the standard material program on every model texture. |
| ncl_fx program weapon_finish | Use the weapon-finish program while retaining the current scalar settings. |
| ncl_fx pvs 0 / ncl_fx pvs 1 | Disable or enable explicit outside-PVS transmission. |
| ncl_fx set parameter value | Set a parameter using the human-readable values below. The plugin sends finite floats in the parameter's units. |
| ncl_fx status | Print the entity index, model, program, binding result, transmission flag, readiness and current float parameter values. |
| ncl_fx remove | Clear and remove the target. |
| ncl_fx_check_numbers | Check numeric output without a target; read-only. |

`ncl_fx_check_numbers` runs from the server console or a connected player's console. It checks formatting and parsing of seven float values by comparing their bit representations. It requires no target and only reads diagnostic values; materials, entities and menu state stay unchanged. Expected summary: `[NCL FX] number formatting: PASS (7/7)`. Small numbers use scientific notation through the standard JSON module.

| Parameter for set | Range |
|---|---|
| `dissolve`, `noise`, `fill`, `glitch` | `0..1` |
| `outline` | `0..16` pixels |
| `glow` | `0..4` |
| `glitch_speed`, `noise_speed` | `0..60` pattern updates per second |
| `glitch_seed`, `noise_seed` | `0..32767` |
| `outline_mode` | `0`: occluded only (default), `1`: always, full silhouette; `2`: visible only |

Glitch includes geometry displacement and RGB separation. Its displacement, band count and RGB width use the manifest defaults and can be changed in the preset/editor. `glitch_speed 0` freezes the pattern. Preset colors are orange fill and blue outline; halo radius 10 pixels. The dissolve mask is a shipped coarse noise mask copied into the plugin resources.

## Visual checks

In a network game, only server parameter commands activate fill and outline. Binding a preset without those commands leaves effects disabled. The `pvs` command controls outside-PVS transmission separately from visible effects. Local `.mat` files and `r_mat_bind`/`r_mat_param` cannot activate network highlighting.

```text
ncl_fx spawn leet
ncl_fx status
```

View the target directly, then place a wall between the camera and target. Visible surfaces keep their normal color. Hidden surfaces receive orange fill and a blue external contour/halo. The wall cut does not create an outline through the model. Move and rotate the camera to check that the contour follows faces without shifting.

Compare outline modes:

```text
ncl_fx set outline_mode 0
ncl_fx set outline_mode 1
ncl_fx set outline_mode 2
```

Mode `0` draws the contour and glow only where the silhouette is hidden by geometry. Mode `1` draws the full external contour and glow, including directly visible parts. Mode `2` draws the contour and glow only on the visible part of the external silhouette. Fill remains limited to hidden surfaces in all three modes. Repeat while the target is fully visible, partially hidden and fully hidden. An outline width of `0` disables both contour and glow.

Isolate fill:

```text
ncl_fx set outline 0
ncl_fx set glow 0
```

Check contour and glow:

```text
ncl_fx set fill 0
ncl_fx set outline 2
ncl_fx set glow 1
```

Check geometry deformation and RGB separation:

```text
ncl_fx set fill 0
ncl_fx set outline 0
ncl_fx set glow 0
ncl_fx set glitch 1
ncl_fx set glitch_speed 12
```

`ncl_fx set glitch_speed 0` freezes the pattern. Change `glitch_seed` for a different deformation, or set `glitch 0` to disable it. Test `ncl_fx set dissolve 0.5` and `ncl_fx set noise 0.5` separately, then combine them with fill/outline/glitch. Setting either amount to `1` removes the complete surface and its effect coverage. Repeat with `ncl_fx program weapon_finish`, `ncl_fx spawn ak47` and `ncl_fx spawn chick`.

For outside-PVS checks, leave the target in another room or beyond a map leaf boundary and compare `ncl_fx pvs 1` with `ncl_fx pvs 0`. A second vanilla client should retain normal visibility behavior. GameDLL rejection rules and engine packet capacity remain in force. The GameDLL visibility cache can retain a recently visible entity briefly after disabling the flag. Console status reports the requested transmission flag and material binding result; the visual PVS observation requires connected clients and is not an automated test.

## Build

Create `build/materials-test`. Run the compiler from the repository root, substituting the installed AMX Mod X and NextClientServerApi directories:

```text
amxxpc test_support/plugins/nextclient_materials_test/nextclient_materials_test.sma "-i<amxx>/scripting/include" "-i<api>/addons/amxmodx/scripting/include" "-obuild/materials-test/nextclient_materials_test.amxx"
```

On Windows, use `amxxpc.exe`. In WSL, compile a copy of the source and headers on a native Linux filesystem because the 32-bit AMXX compiler is incompatible with some DrvFs operations. ReAPI is not required. Check the preset with `matgen check` using the current embedded client manifests. The preset uses the exact texture names and case from the stock models.
