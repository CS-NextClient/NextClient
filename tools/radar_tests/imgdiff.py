#!/usr/bin/env python3
"""Tolerant image diff for radar_shot captures.

The advanced radar is rendered with anti-aliasing, an analytic blur of the live 3D
scene, screen fades and the like, so two captures of the "same" frame are never
bit-identical across runs, and never identical across GPUs/drivers/resolutions. A
useful regression check therefore compares a candidate capture against a committed
golden with a per-pixel tolerance and fails only when enough pixels differ by more
than that tolerance, i.e. on a real visual change rather than sampling noise.

Goldens are machine- and resolution-specific: capture them on the same machine and
video mode you run the check on, and recapture them when the GPU, driver or radar
look intentionally changes.

The fraction on its own is not a verdict, and --clusters exists because of a case
where it said the wrong thing: a real defect in the studio shader path came to 0.178%
against a 0.200% limit, a formal PASS. What told it apart from sampling noise was not
its size but its shape. Noise from GPU against CPU skinning lies along silhouettes,
spread over the frame, and averages to about zero because it is as often lighter as
darker. A defect sits in one place and is lighter or darker throughout. --clusters
groups the pixels over tolerance by the tiles they fill densely and reports, per
group, how one-sided its signed difference is; a group that is both dense and
one-sided fails the run whatever the total fraction says.

Usage:
    python imgdiff.py GOLDEN CHECK [--region X,Y,W,H] [--tol N]
                      [--fail-frac F] [--out DIFF.png] [--clusters]

    --region   compare only this rectangle (image pixels, top-left origin); omit to
               compare the whole frame. Use it to isolate the radar box.
    --tol      per-channel difference (0-255) below which a pixel counts as equal
               (default 12, absorbs AA/blur sampling noise).
    --fail-frac  fraction of pixels allowed to exceed --tol before the check fails
               (default 0.002 = 0.2%).
    --out      write a heatmap of the differing pixels for inspection.
    --clusters report the groups and fail on a dense one-sided one. Thresholds:
               --cluster-tile (tile size, default 16), --cluster-density (share of a
               tile over tolerance for it to count as dense, 0.15), --cluster-min
               (pixels in a group, 64), --cluster-bias (one-sidedness, 0.6).

Exit code 0 = within tolerance (pass), 1 = differs (fail), 2 = usage/IO error.
"""

import argparse
import sys

try:
    import numpy as np
    from PIL import Image
except ImportError:
    sys.stderr.write(
        "imgdiff: requires Pillow and numpy (pip install pillow numpy)\n"
    )
    sys.exit(2)


def LoadRgb(path):
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.int16)


def SliceRegion(a, b, region):
    """Cut both images to the region, refusing one that reaches outside them.

    Validated rather than sliced blindly: numpy clamps a partial overlap and wraps a
    negative origin to the opposite edge, either of which would produce a normal-looking
    verdict over the wrong pixels.
    """
    x, y, w, h = region
    height, width = a.shape[:2]

    if x < 0 or y < 0 or w <= 0 or h <= 0 or x + w > width or y + h > height:
        raise ValueError(f"region {x},{y},{w},{h} lies outside the image")

    return a[y : y + h, x : x + w], b[y : y + h, x : x + w]


def ParseRegion(text):
    parts = [int(v) for v in text.split(",")]
    if len(parts) != 4:
        raise ValueError("region must be X,Y,W,H")
    return parts


def CompareImages(golden, check, region=None, tol=12, fail_frac=0.002, out=None, pair=None):
    """Compare two images with per-pixel tolerance.

    Returns (passed, summary). Raises OSError if an image cannot be read; returns
    (False, ...) on a size mismatch. pair, when given, is the already loaded
    (golden, check) arrays, so one invocation does not decode the files twice.
    """
    a, b = pair if pair is not None else (LoadRgb(golden), LoadRgb(check))
    if a.shape != b.shape:
        return False, (
            f"size mismatch golden={a.shape[1]}x{a.shape[0]} "
            f"check={b.shape[1]}x{b.shape[0]} (recapture the golden at this resolution)"
        )

    if region:
        a, b = SliceRegion(a, b, region)

    per_pixel = np.abs(a - b).max(axis=2)
    over = per_pixel > tol
    frac = float(over.mean())

    if out:
        heat = np.zeros((*per_pixel.shape, 3), dtype=np.uint8)
        scaled = np.clip(per_pixel.astype(np.int32) * 3, 0, 255).astype(np.uint8)
        heat[..., 0] = scaled
        heat[..., 1] = np.where(over, 0, scaled)
        Image.fromarray(heat).save(out)

    passed = frac <= fail_frac
    summary = (
        f"over_tol={int(over.sum())}/{int(over.size)} ({frac * 100:.3f}%)  "
        f"limit={fail_frac * 100:.3f}%  max_delta={int(per_pixel.max())}  tol={tol}"
    )
    return passed, summary


def TileSums(over, signed, tile):
    """Per tile: how many pixels are over tolerance, and their signed differences.

    A tile grid rather than whole rows or columns, because the two things being told
    apart can be in the frame at once: a haze along the silhouettes spans every column,
    and any band wide enough to hold it also holds the compact region, which then
    averages away inside it. A tile is small enough that a dense region fills its own
    tiles while the haze fills none.
    """
    height, width = over.shape
    pad_y = (-height) % tile
    pad_x = (-width) % tile

    if pad_y or pad_x:
        over = np.pad(over, ((0, pad_y), (0, pad_x)))
        signed = np.pad(signed, ((0, pad_y), (0, pad_x)))

    rows = over.shape[0] // tile
    columns = over.shape[1] // tile

    def Fold(values):
        return values.reshape(rows, tile, columns, tile).sum(axis=(1, 3))

    counts = Fold(over.astype(np.int64))
    sums = Fold(np.where(over, signed, 0.0))
    absolute = Fold(np.where(over, np.abs(signed), 0.0))

    return counts, sums, absolute


def GroupTiles(dense):
    """Four-connected groups of dense tiles, each as a list of (row, column)."""
    seen = np.zeros(dense.shape, dtype=bool)
    groups = []

    for start_row, start_column in zip(*np.nonzero(dense)):
        if seen[start_row, start_column]:
            continue

        seen[start_row, start_column] = True
        stack = [(start_row, start_column)]
        group = []

        while stack:
            row, column = stack.pop()
            group.append((row, column))

            for next_row, next_column in (
                (row - 1, column),
                (row + 1, column),
                (row, column - 1),
                (row, column + 1),
            ):
                if not (0 <= next_row < dense.shape[0] and 0 <= next_column < dense.shape[1]):
                    continue
                if dense[next_row, next_column] and not seen[next_row, next_column]:
                    seen[next_row, next_column] = True
                    stack.append((next_row, next_column))

        groups.append(group)

    return groups


def AnalyzeClusters(golden, check, region, tol, tile, minimum, density, bias, pair=None):
    """Clusters of dense differing pixels, and how many pixels lay outside them."""
    a, b = pair if pair is not None else (LoadRgb(golden), LoadRgb(check))

    if region:
        a, b = SliceRegion(a, b, region)

    over = np.abs(a - b).max(axis=2) > tol
    signed = (b - a).mean(axis=2)

    counts, sums, absolute = TileSums(over, signed, tile)
    dense = counts >= density * tile * tile

    clusters = []

    for group in GroupTiles(dense):
        rows = [row for row, _ in group]
        columns = [column for _, column in group]

        count = int(sum(counts[row, column] for row, column in group))
        total = float(sum(sums[row, column] for row, column in group))
        total_absolute = float(sum(absolute[row, column] for row, column in group))

        cluster = {
            "x0": min(columns) * tile,
            "x1": (max(columns) + 1) * tile - 1,
            "y0": min(rows) * tile,
            "y1": (max(rows) + 1) * tile - 1,
            "tiles": len(group),
            "count": count,
            "density": count / (len(group) * tile * tile),
            "mean": total / count if count else 0.0,
            # One-sidedness: 1 or -1 when every differing pixel moved the same way, about
            # 0 when as many went up as down, which is what sampling noise does.
            "bias": total / total_absolute if total_absolute else 0.0,
        }

        cluster["concentrated"] = count >= minimum and abs(cluster["bias"]) >= bias
        clusters.append(cluster)

    clusters.sort(key=lambda cluster: cluster["count"], reverse=True)
    spread = int(over.sum()) - sum(cluster["count"] for cluster in clusters)

    return clusters, spread


def Main():
    ap = argparse.ArgumentParser(description="Tolerant image diff for radar captures.")
    ap.add_argument("golden")
    ap.add_argument("check")
    ap.add_argument("--region", type=ParseRegion, default=None)
    ap.add_argument("--tol", type=int, default=12)
    ap.add_argument("--fail-frac", type=float, default=0.002)
    ap.add_argument("--out", default=None)
    ap.add_argument("--clusters", action="store_true")
    ap.add_argument("--cluster-tile", type=int, default=16)
    ap.add_argument("--cluster-min", type=int, default=64)
    ap.add_argument("--cluster-density", type=float, default=0.15)
    ap.add_argument("--cluster-bias", type=float, default=0.6)
    ap.add_argument("--cluster-list", type=int, default=10)
    args = ap.parse_args()

    try:
        pair = (LoadRgb(args.golden), LoadRgb(args.check))

        passed, summary = CompareImages(
            args.golden, args.check, args.region, args.tol, args.fail_frac, args.out, pair
        )

        clusters, spread = (
            AnalyzeClusters(
                args.golden,
                args.check,
                args.region,
                args.tol,
                args.cluster_tile,
                args.cluster_min,
                args.cluster_density,
                args.cluster_bias,
                pair,
            )
            if args.clusters and "size mismatch" not in summary
            else ([], 0)
        )
    except (OSError, ValueError) as e:
        sys.stderr.write(f"imgdiff: {e}\n")
        return 2

    concentrated = [cluster for cluster in clusters if cluster["concentrated"]]
    passed = passed and not concentrated

    print(f"{'PASS' if passed else 'FAIL'}  {summary}")

    for index, cluster in enumerate(clusters[: args.cluster_list]):
        print(
            f"  cluster {index:2d}  x={cluster['x0']}..{cluster['x1']}  "
            f"y={cluster['y0']}..{cluster['y1']}  px={cluster['count']}  "
            f"dens={cluster['density']:.2f}  mean={cluster['mean']:+.1f}  "
            f"bias={cluster['bias']:+.2f}  "
            f"{'CONCENTRATED' if cluster['concentrated'] else 'mixed'}"
        )

    if len(clusters) > args.cluster_list:
        print(f"  and {len(clusters) - args.cluster_list} further clusters, not listed")

    if args.clusters:
        print(f"  {spread} px over tolerance in no dense tile")

    if concentrated:
        print(
            f"  {len(concentrated)} concentrated one-sided cluster(s): a difference of "
            f"this shape is a defect rather than skinning noise, whatever the fraction "
            f"above says"
        )

    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(Main())
