import argparse
import math
from pathlib import Path
import subprocess
import tempfile

from repeat_test import ReadPng, WriteModel


def Check(matgen, directory):
    uv = [(10, 10), (85, 10), (10, 85)]
    identity = (-1, (0, 0, 0, 0, 0, 0))
    quarter_turn = math.pi / 2
    cases = {
        "identity": {},
        "translation": {
            "bones": [identity, (-1, (10, 0, 0, 0, 0, 0))],
            "positions": (0, 0, 0, -9, 0, 0, 0, 1, 0),
            "position_bones": (0, 1, 0),
        },
        "rotation": {
            "bones": [identity, (-1, (0, 0, 0, 0, 0, quarter_turn))],
            "positions": (0, 0, 0, 0, -1, 0, 0, 1, 0),
            "normals": (0, 0, 1, 0, -1, 0, 0, 1, 0),
            "position_bones": (0, 1, 0),
            "normal_bones": (0, 1, 0),
        },
        "hierarchy": {
            "bones": [identity, (-1, (10, 0, 0, 0, 0, quarter_turn)), (1, (0, 9, 0, 0, 0, 0))],
            "positions": (0, 0, 0, 0, 0, 0, 0, 1, 0),
            "position_bones": (0, 2, 0),
        },
        "independent_normal_bone": {
            "bones": [identity, (-1, (10, 0, 0, 0, 0, 0)), (-1, (80, 90, 100, 0, 0, quarter_turn))],
            "positions": (0, 0, 0, -9, 0, 0, 0, 1, 0),
            "normals": (0, 0, 1, 0, -1, 0, 0, 1, 0),
            "position_bones": (0, 1, 0),
            "normal_bones": (0, 2, 0),
        },
    }
    images = {}
    for name, arguments in cases.items():
        folder = directory / name
        folder.mkdir()
        WriteModel(folder / "model.mdl", uv, **arguments)
        result = subprocess.run(
            [str(matgen), "bake", "edges", str(folder / "model.mdl"), "--out", str(folder)],
            text=True, capture_output=True, timeout=30,
        )
        assert result.returncode == 0, (name, result.stdout, result.stderr)
        images[name] = ReadPng(folder / "seam_bmp_edges.png")

    failures = []
    for name, image in images.items():
        different = sum(a != b for a, b in zip(images["identity"][3], image[3]))
        print(name, "different pixels", different, flush=True)
        if image != images["identity"]:
            failures.append(name)
    assert not failures, ("reference pose changes equivalent geometry", failures)

    for name, texture, uv_name, edge_name in (
        ("plain_name", b"A_Z-09", "A_Z-09", "A_Z-09"),
        ("sanitized_name", b"../A:B.bmp", "___A_B_bmp", "___A_B_bmp"),
        ("empty_name", b"", "texture", ""),
    ):
        folder = directory / name
        folder.mkdir()
        WriteModel(folder / "model.mdl", uv, texture_name=texture)
        for arguments, filename, channels in (
            (["uv"], uv_name + "_uv.png", 4),
            (["bake", "edges"], edge_name + "_edges.png", 1),
        ):
            result = subprocess.run(
                [str(matgen), *arguments, str(folder / "model.mdl"), "--out", str(folder)],
                text=True, capture_output=True, timeout=30,
            )
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            assert ReadPng(folder / filename)[:3] == (100, 100, channels)
        print(name, "output names preserved", flush=True)
    print("PASS: translation, rotation, hierarchy, and independent normal bones")


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument("matgen", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="matgen-reference-pose-") as temporary:
        Check(args.matgen.resolve(), Path(temporary))


if __name__ == "__main__":
    Main()
