# Compare rendering images

[Russian translation](image-comparison.ru.md)

`imgdiff.py` compares prepared reference and candidate images with per-pixel tolerances. It uses Python, NumPy, and Pillow and accepts image formats supported by the installed Pillow, including PNG and TGA. The script compares RGB values.

For radar captures, prepare scenes with the TestLab plugin and follow the [manual testing procedure](../testing/manual-testing.md) and [scenario checklist](../testing/manual-testing-scenarios.md).

## Compare images

1. Prepare reference and candidate captures of the same fixed scene at the same resolution and with matching graphics settings. Record the GPU and driver with the references.
2. From the repository root, run the comparison using paths to the prepared files:

```text
python tools/radar_tests/imgdiff.py "<captures>/golden.tga" "<captures>/check.tga" --out diff.png
```

Replace `<captures>` with the directory containing the images. Both images must have the same dimensions. `--out` saves a heatmap for the compared image or selected region. Update references after an intentional rendering or hardware change.

## Tolerances and regions

For each pixel, the script takes the largest absolute difference among its RGB channels. A pixel differs when that value exceeds `--tol`, which defaults to `12`. The comparison passes when the fraction of differing pixels is at most `--fail-frac`, which defaults to `0.002` (0.2%). A size mismatch fails the comparison.

`--region X,Y,W,H` restricts comparison to a rectangle measured from the image's upper-left corner. The origin must be nonnegative, the width and height must be positive, and the rectangle must fit entirely within the images.

## Inspect concentrated differences

```text
python tools/radar_tests/imgdiff.py "<captures>/golden.tga" "<captures>/check.tga" --clusters --out diff.png
```

`--clusters` groups adjacent tiles densely filled with differing pixels. A group with enough differing pixels and a strong common direction in their average RGB change also fails the comparison, even when the overall differing-pixel fraction passes.

| Option | Default | Meaning |
|---|---|---|
| `--cluster-tile` | `16` | Tile side in pixels. |
| `--cluster-density` | `0.15` | Minimum differing-pixel fraction for a dense tile. |
| `--cluster-min` | `64` | Minimum differing-pixel count for a group to fail. |
| `--cluster-bias` | `0.6` | Minimum absolute ratio of the sum of signed mean RGB changes to the sum of their absolute values for a group to fail. |
| `--cluster-list` | `10` | Maximum number of groups printed in the report. |

Inspect each reported region and its cause. Animation, movement, lighting, sampling precision, and rendering defects can all change the image. Keep the captures, heatmap, and comparison output together.

## Exit codes

| Code | Meaning |
|---|---|
| `0` | All enabled comparison criteria passed. |
| `1` | Pixel tolerance, image-size, or cluster comparison failed. |
| `2` | Invalid arguments, missing dependencies, invalid region, or image read/write error. |
