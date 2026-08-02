#!/usr/bin/env python3
"""Make studio models that carry chosen texture flags.

The shader studio path branches on the per-texture flags of a model (chrome, flat
shade, full bright, masked, additive), and stock Counter-Strike content sets almost
none of them: of the 2731 models in a full install, 1616 have no flags at all. That
leaves the branches untested unless suitable content happens to be installed.

Rather than author models, this copies a known-good one and rewrites only the flag
field of its textures, so the result differs from a model the game already renders
correctly in exactly one respect.

Usage:
    python mdl_flags.py TEMPLATE.mdl OUTDIR [--only-first]

Writes one variant per flag plus a mixed one. Flags live in the companion <name>T.mdl
when the model keeps its textures there, and that file is rewritten instead.
"""

import os
import shutil
import struct
import sys

STUDIO_VERSION = 10

FLAGS = {
    "flatshade": 0x0001,
    "chrome": 0x0002,
    "fullbright": 0x0004,
    "masked": 0x0040,
    "additive": 0x0020,
}

TEXTURE_ENTRY = 80
FLAGS_OFFSET = 64  # char name[64]; int flags;


def read_header(data):
    if len(data) < 212:
        raise SystemExit("file is shorter than a studio header")

    version = struct.unpack_from("<i", data, 4)[0]
    if version != STUDIO_VERSION:
        raise SystemExit(f"unsupported studio version {version}")

    return {
        "numtextures": struct.unpack_from("<i", data, 180)[0],
        "textureindex": struct.unpack_from("<i", data, 184)[0],
    }


def texture_names(data, header):
    names = []
    for i in range(header["numtextures"]):
        base = header["textureindex"] + i * TEXTURE_ENTRY
        names.append(data[base:base + 64].split(b"\0")[0].decode("latin1"))

    return names


def set_flags(data, header, value, only_first):
    out = bytearray(data)

    for i in range(header["numtextures"]):
        if only_first and i != 0:
            continue

        offset = header["textureindex"] + i * TEXTURE_ENTRY + FLAGS_OFFSET
        struct.pack_into("<i", out, offset, value)

    return bytes(out)


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)

    template = sys.argv[1]
    outdir = sys.argv[2]
    only_first = "--only-first" in sys.argv[3:]

    data = open(template, "rb").read()
    header = read_header(data)

    # Textures may live in a companion file; that is where the flags are then.
    flag_carrier = template
    if header["textureindex"] == 0:
        flag_carrier = template[:-4] + "T.mdl"
        if not os.path.exists(flag_carrier):
            raise SystemExit(f"{template} keeps textures in {flag_carrier}, which is missing")

        data = open(flag_carrier, "rb").read()
        header = read_header(data)

    print(f"template: {template}")
    print(f"flags in: {flag_carrier}")
    print(f"textures: {', '.join(texture_names(data, header))}")

    os.makedirs(outdir, exist_ok=True)

    variants = dict(FLAGS)
    variants["chrome_flat"] = FLAGS["chrome"] | FLAGS["flatshade"]
    variants["plain"] = 0

    # A second flag-free copy, under its own name and for one reason: the material sample
    # is authored next to the plain one, and a material is drawn only by the shader path.
    # Anything judged by comparing the two paths has to sit on a model no material reaches.
    variants["shell"] = 0

    stem = os.path.splitext(os.path.basename(template))[0]

    for name, value in sorted(variants.items()):
        patched = set_flags(data, header, value, only_first)

        if flag_carrier == template:
            path = os.path.join(outdir, f"{stem}_{name}.mdl")
            open(path, "wb").write(patched)
        else:
            path = os.path.join(outdir, f"{stem}_{name}.mdl")
            shutil.copyfile(template, path)
            open(os.path.join(outdir, f"{stem}_{name}T.mdl"), "wb").write(patched)

        print(f"  {os.path.basename(path)}  flags=0x{value:04x}")


if __name__ == "__main__":
    main()
