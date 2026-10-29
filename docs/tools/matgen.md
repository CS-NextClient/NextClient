# matgen

[Russian translation](matgen.ru.md) | [Authoring guide](../weapon-materials-authoring.md) | [Material format](../weapon-materials.md)

`matgen` exports model UV templates, validates material files, and bakes model-specific wear maps. The tool uses `ncl_material` and `ncl_studio` and runs outside the game engine.

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

The command writes a wireframe PNG for every texture record in the model. `--skin` selects the family used to assign mesh triangles to those textures; unused textures receive empty templates. The default family is `0`.

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

The command bakes a wear-threshold map from model geometry for each texture used by triangles in the selected skin family. Unused textures are skipped. Empty texture names produce `_edges.png`; collision checks include only images that will be written. Convex regions wear earlier. Curvature is normalized against the model's distribution, and the result extends beyond UV islands to reduce filtering seams. Assign the result to `uWearMap`.

Curvature uses the reference pose from each bone's default values and parent hierarchy. Positions and normals are transformed to model space using their respective bone indices before corners are merged. Normal transforms include rotation only. The baked map represents this fixed pose.

`--out`, `--size`, and `--skin` use the directory, sizing, and family rules described for UV export. Choose a final image size within the client's 1,048,576-pixel area limit.

## Source layout

| Source | Responsibility |
|---|---|
| `tools/matgen/src/Model.cpp` | Read `.mdl` and its texture companion when needed. |
| `tools/matgen/src/manifests.cpp` | Provide embedded program manifests. |
| `tools/matgen/src/Png.cpp` | Write PNG through zlib. |
| `tools/matgen/src/uv_template.cpp` | Export UV wireframes. |
| `tools/matgen/src/check.cpp` | Validate documents, presets, and image metadata. |
| `tools/matgen/src/bake_edges.cpp` | Bake wear-threshold maps. |
