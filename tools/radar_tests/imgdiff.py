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

Usage:
    python imgdiff.py GOLDEN CHECK [--region X,Y,W,H] [--tol N]
                      [--fail-frac F] [--out DIFF.png]

    --region   compare only this rectangle (image pixels, top-left origin); omit to
               compare the whole frame. Use it to isolate the radar box.
    --tol      per-channel difference (0-255) below which a pixel counts as equal
               (default 12, absorbs AA/blur sampling noise).
    --fail-frac  fraction of pixels allowed to exceed --tol before the check fails
               (default 0.002 = 0.2%).
    --out      write a heatmap of the differing pixels for inspection.

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


def load_rgb(path):
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.int16)


def parse_region(text):
    parts = [int(v) for v in text.split(",")]
    if len(parts) != 4:
        raise ValueError("region must be X,Y,W,H")
    return parts


def compare_images(golden, check, region=None, tol=12, fail_frac=0.002, out=None):
    """Compare two images with per-pixel tolerance.

    Returns (passed, summary). Raises OSError if an image cannot be read; returns
    (False, ...) on a size mismatch.
    """
    a = load_rgb(golden)
    b = load_rgb(check)
    if a.shape != b.shape:
        return False, (
            f"size mismatch golden={a.shape[1]}x{a.shape[0]} "
            f"check={b.shape[1]}x{b.shape[0]} (recapture the golden at this resolution)"
        )

    if region:
        x, y, w, h = region
        a = a[y : y + h, x : x + w]
        b = b[y : y + h, x : x + w]

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


def main():
    ap = argparse.ArgumentParser(description="Tolerant image diff for radar captures.")
    ap.add_argument("golden")
    ap.add_argument("check")
    ap.add_argument("--region", type=parse_region, default=None)
    ap.add_argument("--tol", type=int, default=12)
    ap.add_argument("--fail-frac", type=float, default=0.002)
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    try:
        passed, summary = compare_images(
            args.golden, args.check, args.region, args.tol, args.fail_frac, args.out
        )
    except OSError as e:
        sys.stderr.write(f"imgdiff: {e}\n")
        return 2

    print(f"{'PASS' if passed else 'FAIL'}  {summary}")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
