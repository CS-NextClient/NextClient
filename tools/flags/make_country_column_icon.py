"""Draws the server browser country column header icon (a globe) as a 16x16 32-bit TGA in the
color of the stock platform/servers column icons.

Usage: python make_country_column_icon.py [<out-file>]
The output defaults to assets/platform/servers/icon_country_column.tga of this repository.
"""

import pathlib
import sys

from PIL import Image, ImageDraw

ICON_SIZE = 16
SUPERSAMPLE = 8
COLOR = (216, 222, 211)


def draw_globe() -> Image.Image:
    size = ICON_SIZE * SUPERSAMPLE
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    line = int(1.2 * SUPERSAMPLE)
    margin = int(2.5 * SUPERSAMPLE)
    box = (margin, margin, size - margin - 1, size - margin - 1)
    center = size // 2

    draw.ellipse(box, outline=COLOR, width=line)
    draw.line((box[0], center, box[2], center), fill=COLOR, width=line)

    meridian_half_width = (box[2] - box[0]) // 5

    draw.ellipse((center - meridian_half_width, box[1], center + meridian_half_width, box[3]), outline=COLOR, width=line)

    return image.resize((ICON_SIZE, ICON_SIZE), Image.LANCZOS)


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parents[2]
    out_file = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else repo_root / "assets" / "platform" / "servers" / "icon_country_column.tga"

    out_file.parent.mkdir(parents=True, exist_ok=True)

    draw_globe().save(out_file, format="TGA", rle=False)
    print(f"written {out_file}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
