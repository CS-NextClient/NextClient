# Shipped material image collection

[Russian translation](material-masks.ru.md) | [Authoring guide](../weapon-materials-authoring.md)

The collection is stored in `assets/cstrike/materials/` and copied with the game assets by `BUILD_ALL` when `NEXTCLIENT_INSTALL_DIR` is configured. A material references files relative to the game directory, for example `materials/masks/wear/even.png`. Each distinct image file counts toward the document or preset limit, including files shipped with the client. Repeated references to one file count once.

## Compatibility contract

Published paths and image contents remain stable because materials depend on them. Add new images under new names. Changes to `make_masks.py` must preserve the output of existing collection files.

## Contents

| Directory under `materials/` | Contents | Typical input |
|---|---|---|
| `masks/wear/` | Wear-threshold maps. | `uWearMap` |
| `masks/noise/` | Noise at several frequencies. | `uWearMap`, `uPattern` |
| `masks/scratches/` | Fine and coarse, directional and random scratches. | `uWearMap` |
| `masks/dirt/` | Dirt, drips, and smudges. | `uWearMap` |
| `masks/reflect/` | Reflection-strength masks. White reflects; black suppresses reflection. | `uReflectMask` |
| `surfaces/` | Exposed metal, brass, and plastic surfaces. | `uUnderlay` |
| `matcaps/` | View-dependent reflection images. | `uSphereMap` |

## Image conventions

Wear thresholds are dark where wear starts and light where coating persists. Reflection masks use a broad contrast range to distinguish reflective and matte regions. Surface images provide underlay color.

Masks and surface images tile seamlessly. Their noise fields are periodic, and strokes continue into neighboring copies so the edges align when a texture repeats. Matcaps depict a lit sphere and use view-dependent coordinates. All collection images are 512x512 pixels. Threshold masks are grayscale because the shader reads their red channel; surface and matcap images are colored. The runtime image area limit is 1,048,576 pixels.

## Generate

Run from the repository root with Python, NumPy, and Pillow installed:

```text
python tools/material_masks/make_masks.py
```

The script uses a separate deterministic seed per directory. Compare regenerated existing files with the tracked images to verify that their contents remain stable.
