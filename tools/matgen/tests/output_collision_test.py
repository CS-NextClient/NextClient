import argparse
from pathlib import Path
import subprocess
import tempfile

from repeat_test import ReadPng, WriteModel


def Check(matgen, directory):
    uv = [(10, 10), (85, 10), (10, 85)]
    cases = (
        ("sanitized", [b"a.b", b"a_b"], "a_b"),
        ("case", [b"Texture", b"texture"], "Texture"),
        ("empty", [b"", b"texture"], "texture"),
        ("duplicate_empty", [b"", b""], ""),
        ("skipped", [b"a.b", b"a_b"], "a_b"),
    )
    failures = []
    for name, textures, stem in cases:
        for mode, arguments in (("uv", ["uv"]), ("edges", ["bake", "edges"])):
            folder = directory / (name + "_" + mode)
            folder.mkdir()
            WriteModel(folder / "model.mdl", uv, texture_names=textures, used_textures=1 if name == "skipped" else None)
            collision = name not in ("empty", "skipped") or mode == "uv"
            output_stem = "texture" if name == "duplicate_empty" and mode == "uv" else stem
            destination = folder / (output_stem + "_" + mode + ".png")
            sentinel = b"preexisting output must survive rejected export"
            destination.write_bytes(sentinel)
            result = subprocess.run(
                [str(matgen), *arguments, str(folder / "model.mdl"), "--out", str(folder)],
                text=True, capture_output=True, timeout=30,
            )
            if collision:
                rejected = result.returncode != 0 and "collision" in result.stderr.lower()
                preserved = destination.read_bytes() == sentinel
                complete = sorted(path.name for path in folder.glob("*.png")) == [destination.name]
                if not (rejected and preserved and complete):
                    failures.append((name, mode, result.returncode, preserved, complete, result.stderr))
            else:
                assert result.returncode == 0, (name, mode, result.stderr)
                for output in (("a_b_edges.png",) if name == "skipped" else ("_edges.png", "texture_edges.png")):
                    image = ReadPng(folder / output)
                    assert image[:3] == (100, 100, 1)
                    assert any(value != 128 for value in image[3]), output
            print(name, mode, "checked", flush=True)
    assert not failures, ("colliding destinations must fail before writing", failures)
    print("PASS: 10 destination cases, including Windows case and empty-name fallbacks")


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument("matgen", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="matgen-output-collision-") as temporary:
        Check(args.matgen.resolve(), Path(temporary))


if __name__ == "__main__":
    Main()
