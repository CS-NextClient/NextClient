#!/usr/bin/env python3
"""Write a sample material and its artwork, so the material path has something to draw.

Produces a pattern texture and a .mat file next to a model. The model is not modified:
the material lives beside it, which is why the model's CRC still matches whatever the
server expects.

Usage:
    python make_finish_sample.py MODEL.mdl TEXTURE_NAME [--out-texture PATH]
"""

import argparse
import json
import math
import os
import random

from PIL import Image

SIZE = 256


def Pattern():
    """Diagonal stripes with a transparent gap, so both the mix amount and the pattern's
    own alpha are exercised rather than a flat overlay."""
    image = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    pixels = image.load()

    for y in range(SIZE):
        for x in range(SIZE):
            band = ((x + y) % 64) / 64.0

            if band > 0.5:
                continue

            t = math.sin(band * math.pi * 2.0) * 0.5 + 0.5
            pixels[x, y] = (int(220 * t + 30), int(40 * t + 10), int(60 * t + 20), 255)

    return image


def PatternNeutral():
    """The same stripes without a color of their own.

    A tint multiplies, so it can only take color away: over the red pattern above, any
    tint stays red and tells an observer nothing about whether it was applied. Neutral
    artwork is what makes a tint mean something.
    """
    image = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    pixels = image.load()

    for y in range(SIZE):
        for x in range(SIZE):
            band = ((x + y) % 64) / 64.0

            if band > 0.5:
                continue

            t = math.sin(band * math.pi * 2.0) * 0.5 + 0.5
            value = int(215 * t + 40)
            pixels[x, y] = (value, value, value, 255)

    return image


def Underlay():
    """Stand in for whatever the finish wears down to: brushed brass with fine streaks.

    Warm rather than gray on purpose. The model's own skin is gray, so a gray underlay
    would be indistinguishable from an untouched surface and the sample would prove
    nothing about whether the wear reached the pixel.
    """
    image = Image.new("RGB", (SIZE, SIZE))
    pixels = image.load()
    rng = random.Random(1)

    for y in range(SIZE):
        streak = rng.randint(-14, 14)
        for x in range(SIZE):
            level = streak + rng.randint(-6, 6)
            pixels[x, y] = (
                min(255, max(0, 176 + level)),
                min(255, max(0, 128 + level)),
                min(255, max(0, 48 + level)),
            )

    return image


def WearMap():
    """Threshold per texel: where the surface gives way first.

    A smooth field rather than noise, so raising the wear scalar walks the surface in a
    readable order instead of dissolving it at random.
    """
    image = Image.new("RGB", (SIZE, SIZE))
    pixels = image.load()

    for y in range(SIZE):
        for x in range(SIZE):
            u = x / (SIZE - 1.0)
            v = y / (SIZE - 1.0)
            field = 0.5 + 0.35 * math.sin(u * math.pi * 3.0) * math.cos(v * math.pi * 2.0)
            field += 0.1 * math.sin((u + v) * math.pi * 7.0)
            value = int(max(0.0, min(1.0, field)) * 255)
            pixels[x, y] = (value, value, value)

    return image


def SphereMap():
    """Two highlights over a dark field.

    Contrast is what makes the sphere map readable: a bright core against a dark surround
    slides visibly as the model turns, where a uniformly light image only brightens the
    surface and looks like a lighter paint. The second, smaller highlight tells rotation
    apart from a change in brightness.
    """
    image = Image.new("RGB", (SIZE, SIZE))
    pixels = image.load()

    lobes = ((0.36, 0.30, 2.1, 1.0), (0.70, 0.66, 4.0, 0.55))

    for y in range(SIZE):
        for x in range(SIZE):
            u = x / (SIZE - 1.0)
            v = y / (SIZE - 1.0)

            t = 0.0
            for cx, cy, falloff, weight in lobes:
                d = math.hypot(u - cx, v - cy)
                t = max(t, weight * max(0.0, 1.0 - d * falloff) ** 2)

            value = int(14 + 241 * t)
            pixels[x, y] = (value, value, min(255, value + 18))

    return image


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument("model")
    parser.add_argument("texture_name", help="name of the model texture the material applies to")
    parser.add_argument("--out-texture", default=None)
    args = parser.parse_args()

    model_dir = os.path.dirname(args.model)
    texture_path = args.out_texture or os.path.join(model_dir, "finish_pattern.png")

    written = {"pattern": texture_path}
    Pattern().save(texture_path)

    for name, maker in (("neutral", PatternNeutral), ("underlay", Underlay), ("wear", WearMap), ("sphere", SphereMap)):
        path = os.path.join(model_dir, f"finish_{name}.png")
        maker().save(path)
        written[name] = path

    for path in written.values():
        print(f"wrote {path}")

    # The path in the material is what the client opens, so it is relative to the game
    # directory rather than to the material file.
    def GameRelative(path):
        relative = path.replace("\\", "/")
        marker = "/cstrike/"

        return relative.split(marker, 1)[1] if marker in relative else relative

    document = {
        "version": 1,
        "materials": {
            args.texture_name: {
                "program": "weapon_finish",
                "params": {
                    "uPattern": {"texture": GameRelative(written["pattern"])},
                    "uUnderlay": {"texture": GameRelative(written["underlay"])},
                    "uWearMap": {"texture": GameRelative(written["wear"])},
                    "uSphereMap": {"texture": GameRelative(written["sphere"])},
                    "uBaseTint": [1.0, 1.0, 1.0, 1.0],
                    "uPatternTint": [1.0, 1.0, 1.0, 1.0],
                    "uLayerMix": 1.0,
                    "uPatternTransform": [4.0, 4.0, 0.0, 0.0],
                    "uWear": 0.375,
                    "uWearSoftness": 0.08,
                    "uReflectivity": 0.55,
                },
            }
        },
    }

    material_path = os.path.splitext(args.model)[0] + ".mat"
    open(material_path, "w", encoding="utf-8").write(json.dumps(document, indent=2) + "\n")
    print(f"wrote {material_path}")


if __name__ == "__main__":
    Main()
