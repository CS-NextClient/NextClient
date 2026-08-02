#!/usr/bin/env python3
"""Build a WAD3 of text textures, one per label.

The test level puts several models side by side that differ only in a texture flag, and
telling them apart in a screenshot or a diff heatmap is otherwise guesswork. Signs in
the world carry the name, so a capture explains itself.

Usage:
    python make_labels.py OUT.wad LABEL [LABEL ...]
"""

import struct
import sys

from PIL import Image, ImageDraw, ImageFont

WIDTH = 128
HEIGHT = 32

BACKGROUND = (16, 16, 16)
FOREGROUND = (255, 220, 90)


def render(text):
    image = Image.new("RGB", (WIDTH, HEIGHT), BACKGROUND)
    draw = ImageDraw.Draw(image)

    # Shrink until it fits: the labels differ a lot in length and a clipped one is worse
    # than a small one.
    for size in range(20, 7, -1):
        try:
            font = ImageFont.truetype("arialbd.ttf", size)
        except OSError:
            font = ImageFont.load_default()

        box = draw.textbbox((0, 0), text, font=font)
        if box[2] - box[0] <= WIDTH - 8 and box[3] - box[1] <= HEIGHT - 6:
            break

    box = draw.textbbox((0, 0), text, font=font)
    x = (WIDTH - (box[2] - box[0])) // 2 - box[0]
    y = (HEIGHT - (box[3] - box[1])) // 2 - box[1]
    draw.text((x, y), text, font=font, fill=FOREGROUND)

    return image


def quantise(image):
    """Down to 256 colours with an explicit palette, which is what a miptex stores."""
    palette = Image.new("P", (1, 1))
    entries = []
    for i in range(256):
        t = i / 255.0
        entries += [
            int(BACKGROUND[0] + (FOREGROUND[0] - BACKGROUND[0]) * t),
            int(BACKGROUND[1] + (FOREGROUND[1] - BACKGROUND[1]) * t),
            int(BACKGROUND[2] + (FOREGROUND[2] - BACKGROUND[2]) * t),
        ]
    palette.putpalette(entries)

    return image.quantize(palette=palette, dither=Image.Dither.NONE), bytes(entries)


def miptex(name, image):
    indexed, palette = quantise(image)

    mips = []
    current = indexed
    for level in range(4):
        if level:
            current = indexed.resize((WIDTH >> level, HEIGHT >> level), Image.Resampling.NEAREST)
        mips.append(current.tobytes())

    header_size = 16 + 4 + 4 + 4 * 4
    offsets = []
    cursor = header_size
    for data in mips:
        offsets.append(cursor)
        cursor += len(data)

    blob = struct.pack("<16sII4I", name.encode("ascii")[:15], WIDTH, HEIGHT, *offsets)
    blob += b"".join(mips)
    blob += struct.pack("<H", 256) + palette + b"\0\0"

    return blob


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)

    out = sys.argv[1]
    labels = sys.argv[2:]

    lumps = []
    for label in labels:
        name = ("lbl_" + label)[:15].lower()
        lumps.append((name, miptex(name, render(label))))

    body = b""
    entries = []
    offset = 12
    for name, blob in lumps:
        entries.append((offset, len(blob), name))
        body += blob
        offset += len(blob)

    directory = b""
    for position, size, name in entries:
        # filepos, disksize, size, type 0x43 (miptex), compression 0, pad, name
        directory += struct.pack("<iiiBBh16s", position, size, size, 0x43, 0, 0, name.encode("ascii"))

    header = struct.pack("<4sii", b"WAD3", len(lumps), 12 + len(body))
    open(out, "wb").write(header + body + directory)

    print(f"wrote {out}: {len(lumps)} textures")
    for _, _, name in entries:
        print(f"  {name}")


if __name__ == "__main__":
    main()
