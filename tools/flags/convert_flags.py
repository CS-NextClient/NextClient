"""Converts PNG country flags into the server browser flag set.

Input: a directory of PNG files named by ISO 3166-1 alpha-2 code (ru.png, ua.png, ...), for
example the famfamfam flag icons (16x11). Every flag is centered on a transparent 16x16 canvas
and written as an uncompressed 32-bit TGA, the layout of the stock platform/servers icons.

Usage: python convert_flags.py <png-dir> [<out-dir>]
The output directory defaults to assets/platform/servers/flags of this repository.
"""

import pathlib
import re
import sys

from PIL import Image

CANVAS_SIZE = (16, 16)
CODE_PATTERN = re.compile(r"^[a-z]{2}$")


def convert(png_path: pathlib.Path, out_dir: pathlib.Path) -> None:
    flag = Image.open(png_path).convert("RGBA")

    if flag.width > CANVAS_SIZE[0] or flag.height > CANVAS_SIZE[1]:
        flag.thumbnail(CANVAS_SIZE, Image.LANCZOS)

    canvas = Image.new("RGBA", CANVAS_SIZE, (0, 0, 0, 0))
    offset = ((CANVAS_SIZE[0] - flag.width) // 2, (CANVAS_SIZE[1] - flag.height) // 2)

    canvas.paste(flag, offset)
    canvas.save(out_dir / f"{png_path.stem.lower()}.tga", format="TGA", rle=False)


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    png_dir = pathlib.Path(sys.argv[1])
    repo_root = pathlib.Path(__file__).resolve().parents[2]
    out_dir = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else repo_root / "assets" / "platform" / "servers" / "flags"

    out_dir.mkdir(parents=True, exist_ok=True)

    converted = 0
    for png_path in sorted(png_dir.glob("*.png")):
        if not CODE_PATTERN.match(png_path.stem.lower()):
            continue

        convert(png_path, out_dir)
        converted += 1

    print(f"{converted} flags written to {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
