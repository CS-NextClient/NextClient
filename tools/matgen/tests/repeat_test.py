import argparse
import ctypes as c
from pathlib import Path
import struct
import subprocess
import tempfile
import time
import zlib


class Header(c.LittleEndianStructure):
    _fields_ = [
        ("id", c.c_int32), ("version", c.c_int32), ("name", c.c_char * 64),
        ("length", c.c_int32), ("bounds", c.c_float * 15),
    ] + [(name, c.c_int32) for name in (
        "flags", "numbones", "boneindex", "numbonecontrollers", "bonecontrollerindex",
        "numhitboxes", "hitboxindex", "numseq", "seqindex", "numseqgroups", "seqgroupindex",
        "numtextures", "textureindex", "texturedataindex", "numskinref", "numskinfamilies",
        "skinindex", "numbodyparts", "bodypartindex", "numattachments", "attachmentindex",
        "soundtable", "soundindex", "soundgroups", "soundgroupindex", "numtransitions",
        "transitionindex",
    )]


class BodyPart(c.LittleEndianStructure):
    _fields_ = [("name", c.c_char * 64), ("nummodels", c.c_int32),
                ("base", c.c_int32), ("modelindex", c.c_int32)]


class Model(c.LittleEndianStructure):
    _fields_ = [("name", c.c_char * 64), ("type", c.c_int32), ("boundingradius", c.c_float)] + [
        (name, c.c_int32) for name in (
            "nummesh", "meshindex", "numverts", "vertinfoindex", "vertindex", "numnorms",
            "norminfoindex", "normindex", "numgroups", "groupindex",
        )
    ]


class Mesh(c.LittleEndianStructure):
    _fields_ = [(name, c.c_int32) for name in ("numtris", "triindex", "skinref", "numnorms", "normindex")]


class Texture(c.LittleEndianStructure):
    _fields_ = [("name", c.c_char * 64)] + [(name, c.c_int32) for name in ("flags", "width", "height", "index")]


assert c.sizeof(Header) == 244
assert c.sizeof(BodyPart) == 76
assert c.sizeof(Model) == 112
assert c.sizeof(Mesh) == 20
assert c.sizeof(Texture) == 80


def WriteModel(path, uv, width=100, height=100, *, bones=None, positions=None, normals=None,
                position_bones=None, normal_bones=None, texture_name=b"seam.bmp", texture_names=None, used_textures=None):
    texture_names = texture_names if texture_names is not None else [texture_name]
    data = bytearray(c.sizeof(Header))

    def Append(block):
        data.extend(b"\0" * (-len(data) % 4))
        offset = len(data)
        data.extend(block)
        return offset

    def Replace(offset, value):
        data[offset:offset + c.sizeof(value)] = bytes(value)

    header = Header()
    header.id = int.from_bytes(b"IDST", "little")
    header.version = 10
    header.name = b"repeat-seam"
    if bones is None:
        bones = [(-1, (0, 0, 0, 0, 0, 0))]
    header.numbones = len(bones)
    bone_data = bytearray()
    for parent, values in bones:
        bone = bytearray(112)
        bone[:4] = b"bone"
        struct.pack_into("<i", bone, 32, parent)
        struct.pack_into("<6i", bone, 40, *([-1] * 6))
        struct.pack_into("<6f", bone, 64, *values)
        bone_data.extend(bone)
    header.boneindex = Append(bone_data)

    body_offset = Append(bytes(c.sizeof(BodyPart)))
    model_offset = Append(bytes(c.sizeof(Model)))
    mesh_offset = Append(bytes(c.sizeof(Mesh) * len(texture_names)))

    model = Model()
    model.name = b"triangle"
    model.nummesh = len(texture_names) if used_textures is None else used_textures
    model.meshindex = mesh_offset
    model.numverts = 3
    model.vertindex = Append(struct.pack("<9f", *(positions or (0, 0, 0, 1, 0, 0, 0, 1, 0))))
    model.numnorms = 3
    model.normindex = Append(struct.pack("<9f", *(normals or (0, 0, 1, 1, 0, 0, 0, 1, 0))))
    model.vertinfoindex = Append(bytes(position_bones or (0, 0, 0)))
    model.norminfoindex = Append(bytes(normal_bones or (0, 0, 0)))

    mesh = Mesh()
    mesh.numtris = 1
    mesh.skinref = 0
    mesh.numnorms = 3
    mesh.triindex = Append(struct.pack("<14h", 3, 0, 0, *uv[0], 1, 1, *uv[1], 2, 2, *uv[2], 0))

    body = BodyPart()
    body.name = b"body"
    body.nummodels = 1
    body.base = 1
    body.modelindex = model_offset
    header.numbodyparts = 1
    header.bodypartindex = body_offset

    header.numtextures = len(texture_names)
    header.textureindex = Append(bytes(c.sizeof(Texture) * len(texture_names)))
    header.numskinref = len(texture_names)
    header.numskinfamilies = 1
    header.skinindex = Append(struct.pack("<" + "h" * len(texture_names), *range(len(texture_names))))

    textures = []
    for name in texture_names:
        texture = Texture()
        texture.name = name
        texture.width = width
        texture.height = height
        texture.index = Append(bytes(width * height + 256 * 3))
        textures.append(texture)
    header.texturedataindex = textures[0].index
    header.length = len(data)

    Replace(0, header)
    Replace(body_offset, body)
    Replace(model_offset, model)
    for index, texture in enumerate(textures):
        mesh.skinref = index
        Replace(mesh_offset + index * c.sizeof(Mesh), mesh)
        Replace(header.textureindex + index * c.sizeof(Texture), texture)

    path.write_bytes(data)


def ReadPng(path):
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    compressed = bytearray()
    offset = 8
    while offset < len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        chunk = data[offset + 8:offset + 8 + length]
        if kind == b"IHDR":
            width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", chunk)
            assert (depth, compression, filtering, interlace) == (8, 0, 0, 0)
            assert color in (0, 6)
            channels = 1 if color == 0 else 4
        elif kind == b"IDAT":
            compressed.extend(chunk)
        offset += 12 + length

    raw = zlib.decompress(compressed)
    stride = width * channels + 1
    assert len(raw) == height * stride
    assert all(raw[y * stride] == 0 for y in range(height))
    pixels = b"".join(raw[y * stride + 1:(y + 1) * stride] for y in range(height))
    return width, height, channels, pixels


def Pixel(image, x, y):
    width, height, channels, pixels = image
    assert 0 <= x < width and 0 <= y < height
    start = (y * width + x) * channels
    return tuple(pixels[start:start + channels])


def Check(matgen, directory):
    cases = {
        "horizontal": [(90, 20), (110, 20), (90, 80)],
        "vertical": [(20, 90), (20, 110), (80, 90)],
        "negative": [(-10, 20), (10, 20), (-10, 80)],
        "translated": [(390, -180), (410, -180), (390, -120)],
        "thin": [(-32768, -32768), (32767, 32767), (32766, 32767)],
        "tiny_texture": [(-32768, -32768), (32767, 32767), (32766, 32767)],
    }
    results = {}
    for name, uv in cases.items():
        folder = directory / name
        folder.mkdir()
        WriteModel(folder / "model.mdl", uv, 1 if name == "tiny_texture" else 100, 1 if name == "tiny_texture" else 100)
        for mode, arguments in (("uv", ["uv"]), ("edges", ["bake", "edges"])):
            start = time.perf_counter()
            run = subprocess.run(
                [str(matgen), *arguments, str(folder / "model.mdl"), "--out", str(folder)],
                capture_output=True, text=True, timeout=30,
            )
            assert run.returncode == 0, (name, mode, run.stdout, run.stderr)
            image = ReadPng(folder / ("seam_bmp_" + mode + ".png"))
            results[name, mode] = image
            print(name, mode, "seconds", round(time.perf_counter() - start, 3), flush=True)
            if name in ("horizontal", "negative", "translated"):
                point = (5, 20) if mode == "uv" else (5, 25)
                value = Pixel(image, *point)
                # Curvatures (0, -1/sqrt(2), -1) and weights (2/15, 31/40, 11/120)
                # give an interpolated threshold of 112 at this wrapped sample.
                assert value[3] > 0 if mode == "uv" else value[0] == 112, (name, mode, point, value)
            if name == "vertical":
                point = (20, 5) if mode == "uv" else (25, 5)
                value = Pixel(image, *point)
                assert value[3] > 0 if mode == "uv" else value[0] == 112, (name, mode, point, value)

    folder = directory / "large_repeat"
    folder.mkdir()
    WriteModel(folder / "model.mdl", cases["thin"], 2, 2)
    start = time.perf_counter()
    run = subprocess.run(
        [str(matgen), "bake", "edges", str(folder / "model.mdl"), "--out", str(folder), "--size", "2048"],
        capture_output=True, text=True, timeout=30,
    )
    assert run.returncode == 0, ("large_repeat", run.stdout, run.stderr)
    image = ReadPng(folder / "seam_bmp_edges.png")
    assert image[:3] == (2048, 2048, 1)
    assert any(value != 128 for value in image[3])
    print("large_repeat edges seconds", round(time.perf_counter() - start, 3), flush=True)

    for mode in ("uv", "edges"):
        for name in ("negative", "translated"):
            assert results["horizontal", mode] == results[name, mode], (name, mode, "whole-tile translation")
    print("PASS: repeat seams, interpolation, negative UVs, tile translations, and thin signed-short ranges")


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument("matgen", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="matgen-repeat-") as temporary:
        Check(args.matgen.resolve(), Path(temporary))


if __name__ == "__main__":
    Main()
