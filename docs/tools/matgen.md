# matgen

[Russian translation](matgen.ru.md) | [Authoring guide](../weapon-materials-authoring.md) | [Material format](../weapon-materials.md)

`matgen` exports model UV templates, validates material files, and bakes model-specific wear maps. The tool runs without starting the game.

## Build

Select a preset from `CMakePresets.json` that supports the installed build tools. For Ninja with MSVC, initialize the x86 compiler environment first.

```text
cmake --preset ninja
cmake --build --preset ninja-debug --target matgen
```

The default Windows output is `out/bin/Debug/matgen.exe`. The output path depends on the selected configuration and CMake settings. In the commands below, replace `matgen` with the path to the built executable unless its directory is on `PATH`.

## Export UV templates

```text
matgen uv <model.mdl> [--out <dir>] [--size <pixels>] [--skin <family>]
```

The command creates a PNG showing the UV layout for each model texture. `--skin` selects the skin family used to produce the templates. Unused textures receive empty templates. The default family is `0`.

Without `--size`, output starts from the source texture dimensions. `--size` scales the longer side. Each output dimension is then clamped to `64..2048` pixels, which can change the aspect ratio at the limits. Files are written beside the model unless `--out` is supplied.

Wireframe coverage wraps at texture repeat boundaries while preserving each triangle's geometry. The tool reports triangles shifted by whole texture periods. Use the exported texture names when assigning materials.

Output names preserve ASCII letters, digits, `_`, and `-`; other characters become `_`. UV export uses `texture` for an empty name. Before writing images, the tool rejects filenames that collide after this conversion or ASCII case folding, including on Windows. Existing output files remain unchanged on this preflight failure.

## Validate a document or preset

```text
matgen check <file.mat> [--game <dir>]
```

The command detects a model document or server preset from its structure, parses it, and checks its materials against embedded program manifests. It also checks image paths, file availability, PNG/TGA metadata, and the area limit of 1,048,576 pixels. The limit applies to `width * height`. Image paths resolve against `--game`, or the current directory when omitted. Exit code `1` indicates a problem; `0` indicates that these checks passed.

The tool uses the same manifest files as the engine. Runtime GL allocation, complete image decoding, custom shader programs, and file delivery from the server require separate checks in the corresponding components.

## Bake edge wear

```text
matgen bake edges <model.mdl> [--out <dir>] [--size <pixels>] [--skin <family>]
```

The command bakes a wear-threshold map from model geometry for each texture used by triangles in the selected skin family. Unused textures are skipped. Empty texture names produce `_edges.png`; collision checks include only images that will be written. Convex regions wear earlier. Map values are scaled separately for each model; matching brightness across different models does not imply matching curvature. Color padding around UV islands reduces seams caused by texture filtering. Assign the result to `uWearMap`.

The wear map is baked from the model's reference pose. Inspect joints and other areas whose curvature changes with animation; the baked image remains fixed while the model moves.

`--out`, `--size`, and `--skin` use the directory, sizing, and family rules described for UV export. Choose a final image size within the client's 1,048,576-pixel area limit.
