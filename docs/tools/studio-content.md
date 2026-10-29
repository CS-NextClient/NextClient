# Prepare Studio rendering test content

[Russian translation](studio-content.ru.md) | [Test cases](../testing/studio-rendering.md) | [Diagnostics](../renderer-diagnostics.md)

These tools generate the `studio_test` map and model variants for developer-run rendering checks. Run preparation from the repository root. The generated files are installed into the specified game directory. Record observations from the installed game separately from source checks and build results.

## Prerequisites

Provide Python with Pillow, a valid GIGN model, `halflife.wad`, and the custom models used by the map. Defaults expect `YoRHa_2B`, `svinorez_t_robb`, and `next21_adventures/treasure.mdl` under the game's search paths. Use `--perf-a` and `--perf-b` for model names under `--perf-models`; each must have the layout `<name>/<name>.mdl`. `--remap-model` selects a game-relative model path, and `--remap-model-file` supplies its on-disk file for height calculation. `--perf-count` defaults to 48.

Place `hlcsg.exe`, `hlbsp.exe`, `hlvis.exe`, and `hlrad.exe` in `<game>/tools/`. The build script expects generated `.map` files under `<game>/cstrike/maps/src/` and copies the compiled BSP into `<game>/cstrike/maps/`.

## Generate and compile

Replace `<game>` with the installation root, then run:

```text
python tools/studio_tests/mdl_flags.py "<game>/cstrike/models/player/gign/gign.mdl" "<game>/cstrike/models/studio_tests"
python tools/studio_tests/make_labels.py "<game>/cstrike/studio_test.wad" plain flatshade chrome chromeflat fullbright masked additive glow_rm4 add_rm5 color_rm1 shell_red shell_blue remap
python tools/studio_tests/make_maps.py "<game>/cstrike/maps/src" --flag-models "<game>/cstrike/models/studio_tests" --perf-models "<game>/cstrike_downloads/models/player" --wad "<game>/valve/halflife.wad" --label-wad "<game>/cstrike/studio_test.wad" --remap-model-file "<game>/cstrike_downloads/models/next21_adventures/treasure.mdl"
powershell -File tools/studio_tests/build_maps.ps1 -GameDir "<game>/cstrike"
```

`mdl_flags.py` copies the source model and changes its texture flags to produce `plain`, `flatshade`, `chrome`, `chrome_flat`, `fullbright`, `masked`, and `additive` variants. If textures are external, it updates the texture companion file. It also creates a flag-free `shell` copy for shell and lighting checks. Bind material samples to the `plain` variant. Use `shell` when comparing models without authored materials.

The WAD must contain a texture for every label on the map. The build script embeds textures, performs full VIS, runs RAD with `-fast`, and installs `studio_test.bsp`. Full VIS and the corridor limit visibility between the two rooms.

## Map layout

| Area | Content |
|---|---|
| South room and spawn | Texture-flag variants, render modes 4/5/1, red and blue glow shells, an AK pickup, and a remap-skin model. |
| Dark alcove | Two material-free models for scene-light and entity-light checks. |
| North room | A 12-column grid alternating the two performance models, with 48 entities by default. |
| S-shaped corridor | Limits visibility between the rooms. Use a fixed north-room viewpoint and confirm the visible model set in the diagnostic dump. |

The remap model's sequence bounding box determines its standing height when `--remap-model-file` is supplied. Its `colormap` is zero in the generated map. To inspect other remap colors, set the entity's `colormap` through a server plugin using `top | (bottom << 8)`.

After preparation, use the [Studio test cases](../testing/studio-rendering.md) and [capture procedures](../renderer-diagnostics.md) in the installed game.
