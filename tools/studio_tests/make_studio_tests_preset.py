#!/usr/bin/env python3
"""Write a sample material preset.

Stands in for what a server ships for a map. The two materials are deliberately
unmistakable against each other in two independent ways, color and tiling: a check
where both outcomes look alike is not a check.

Usage:
    python make_studio_tests_preset.py GAME_DIR/cstrike/materials/studio_tests.mat

Then, in game:
    r_mat_bind <entity> materials/studio_tests.mat finish_cyan
"""

import argparse
import json
import os

ARTWORK = "models/studio_tests"


def Materials():
    return [
        {
            "name": "finish_cyan",
            "texture": "GIGN_DMBASE2.bmp",
            "material": {
                "program": "weapon_finish",
                "params": {
                    "uPattern": {"texture": f"{ARTWORK}/finish_pattern.png"},
                    "uUnderlay": {"texture": f"{ARTWORK}/finish_underlay.png"},
                    "uWearMap": {"texture": f"{ARTWORK}/finish_wear.png"},
                    "uSphereMap": {"texture": f"{ARTWORK}/finish_sphere.png"},
                    "uPatternTint": [0.2, 0.9, 1.0, 1.0],
                    "uPatternTransform": [2.0, 2.0, 0.0, 0.0],
                    "uLayerMix": 1.0,
                    "uWear": 0.625,
                    "uWearSoftness": 0.05,
                    "uReflectivity": 0.75,
                },
            },
        },
        {
            # Neutral artwork, because a tint multiplies and so cannot turn the red
            # pattern into anything but red, which would make the two look alike and
            # prove nothing about which one was applied.
            "name": "finish_gold",
            "texture": "GIGN_DMBASE2.bmp",
            "material": {
                "program": "weapon_finish",
                "params": {
                    "uPattern": {"texture": f"{ARTWORK}/finish_neutral.png"},
                    "uPatternTint": [1.0, 0.8, 0.05, 1.0],
                    "uPatternTransform": [8.0, 8.0, 0.0, 0.0],
                    "uLayerMix": 1.0,
                    # Present but idle: uWear is left out so it defaults to zero, which
                    # is what an instance value then drives. A program only reads its
                    # optional layers once the value that switches them on is non-zero,
                    # so the textures cost nothing until the server asks for wear.
                    "uUnderlay": {"texture": f"{ARTWORK}/finish_underlay.png"},
                    "uWearMap": {"texture": f"{ARTWORK}/finish_wear.png"},
                    "uSphereMap": {"texture": f"{ARTWORK}/finish_sphere.png"},
                    "uReflectivity": 0.6,
                },
            },
        },
    ]


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument("out")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.out), exist_ok=True)

    preset = {"version": 1, "materials": Materials()}
    open(args.out, "w", encoding="utf-8").write(json.dumps(preset, indent=2) + "\n")

    print(f"wrote {args.out}")


if __name__ == "__main__":
    Main()
