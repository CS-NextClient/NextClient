#!/usr/bin/env python3
"""Produce the mask collection the client ships with.

The collection exists so that a material can reference artwork the client already has:
nothing is downloaded for it, and it does not count against the texture budgets a server
pays. That is also why it is generated rather than collected: a file produced by this
script is reproducible, reviewable and free of questions about where it came from.

Everything here is seeded and deterministic, and every image tiles, because the mask
transforms scale a mask over the model's own coordinates and a seam would show at every
repeat.

The convention for a threshold map is that dark wears through before light, so a scratch
is drawn dark on a light field.

The names are the contract. Once a material references materials/masks/wear/even.png the
contents of that file are part of how somebody else's content looks, so a correction is a
new name rather than a new version of an old one.

Usage:
    python tools/material_masks/make_masks.py [--out assets/cstrike/materials]
"""

import argparse
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

SIZE = 512


# --- fields ---------------------------------------------------------------------------


def Smoothstep(t):
    return t * t * (3.0 - 2.0 * t)


def Quintic(t):
    """Interpolation with a zero second derivative at the lattice points.

    Cubic leaves a crease along every lattice line, and on the coarse fields the collection
    leans on most that crease reads as a grid of squares rather than as a surface.
    """
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)


def ValueNoise(rng, period_x, period_y, size=SIZE):
    """A lattice of random values, smoothly interpolated and wrapping at the tile edge.

    The two periods are separate so a field can be stretched: few cells across one axis
    and many across the other is what a brushed or rubbed surface looks like.
    """
    lattice = rng.random((period_y, period_x))

    fx = np.arange(size) * (period_x / size)
    fy = np.arange(size) * (period_y / size)

    x0 = np.floor(fx).astype(np.int64) % period_x
    y0 = np.floor(fy).astype(np.int64) % period_y
    x1 = (x0 + 1) % period_x
    y1 = (y0 + 1) % period_y

    tx = Quintic(fx - np.floor(fx))[None, :]
    ty = Quintic(fy - np.floor(fy))[:, None]

    top = lattice[np.ix_(y0, x0)] * (1.0 - tx) + lattice[np.ix_(y0, x1)] * tx
    bottom = lattice[np.ix_(y1, x0)] * (1.0 - tx) + lattice[np.ix_(y1, x1)] * tx

    return top * (1.0 - ty) + bottom * ty


def Fbm(rng, period_x, period_y, octaves, gain=0.5, size=SIZE):
    total = np.zeros((size, size))
    amplitude = 1.0
    weight = 0.0

    for octave in range(octaves):
        total += amplitude * ValueNoise(rng, period_x << octave, period_y << octave, size)
        weight += amplitude
        amplitude *= gain

    return total / weight


def Cellular(rng, cells, size=SIZE):
    """Distance to the nearest of a set of scattered points, measured around the tile."""
    px = rng.random(cells)
    py = rng.random(cells)

    gx = ((np.arange(size) + 0.5) / size)[None, :]
    gy = ((np.arange(size) + 0.5) / size)[:, None]

    nearest = np.full((size, size), 4.0)

    for i in range(cells):
        dx = np.abs(gx - px[i])
        dy = np.abs(gy - py[i])
        dx = np.minimum(dx, 1.0 - dx)
        dy = np.minimum(dy, 1.0 - dy)
        nearest = np.minimum(nearest, dx * dx + dy * dy)

    return np.sqrt(nearest)


def CellularRegions(rng, cells, size=SIZE):
    """Which scattered point owns each texel, as a flat black or white region.

    Distance to the nearest point thresholds into circles; taking the owner instead gives
    areas that meet along a line, which is what parts finished separately look like.
    """
    px = rng.random(cells)
    py = rng.random(cells)
    lit = (rng.random(cells) > 0.5).astype(np.float64)

    gx = ((np.arange(size) + 0.5) / size)[None, :]
    gy = ((np.arange(size) + 0.5) / size)[:, None]

    nearest = np.full((size, size), 4.0)
    out = np.zeros((size, size))

    for i in range(cells):
        dx = np.abs(gx - px[i])
        dy = np.abs(gy - py[i])
        dx = np.minimum(dx, 1.0 - dx)
        dy = np.minimum(dy, 1.0 - dy)

        distance = dx * dx + dy * dy
        closer = distance < nearest

        nearest = np.where(closer, distance, nearest)
        out = np.where(closer, lit[i], out)

    return out


def Normalize(field):
    low = field.min()
    high = field.max()

    return (field - low) / (high - low) if high > low else np.zeros_like(field)


def Contrast(field, amount):
    """Pushes a field away from its middle, so a smooth cloud reads as separate patches."""
    return np.clip((field - 0.5) * amount + 0.5, 0.0, 1.0)


def Threshold(field, level, softness):
    """Cuts a field into near black and near white with a soft edge between.

    A mask that decides whether something happens at all has to actually decide: one that
    hovers around a single gray only scales the effect evenly everywhere, which is
    indistinguishable from having no mask.
    """
    return Smoothstep(np.clip((field - (level - softness)) / (2.0 * softness), 0.0, 1.0))


def Strokes(rng, count, length, width, blur, angle_range, size=SIZE):
    """Line segments, drawn into every wrap of the tile so the result still repeats.

    Returned as coverage in 0..1, which the caller subtracts from a field: a scratch is a
    place that gives way early, not a place that is brighter.
    """
    canvas = Image.new("L", (size * 3, size * 3), 0)
    draw = ImageDraw.Draw(canvas)

    for _ in range(count):
        x = rng.random() * size
        y = rng.random() * size
        angle = angle_range[0] + rng.random() * (angle_range[1] - angle_range[0])
        span = length * (0.4 + rng.random())

        dx = np.cos(angle) * span * 0.5
        dy = np.sin(angle) * span * 0.5

        for tile_y in range(3):
            for tile_x in range(3):
                ox = tile_x * size + x
                oy = tile_y * size + y
                draw.line((ox - dx, oy - dy, ox + dx, oy + dy), fill=255, width=width)

    if blur > 0:
        canvas = canvas.filter(ImageFilter.GaussianBlur(blur))

    centre = canvas.crop((size, size, size * 2, size * 2))

    return np.asarray(centre, dtype=np.float64) / 255.0


# --- writing --------------------------------------------------------------------------


def WriteGray(field, path):
    image = Image.fromarray(np.clip(field * 255.0 + 0.5, 0, 255).astype(np.uint8), mode="L")
    image.save(path, optimize=True)

    return path


def WriteRgb(field, path):
    image = Image.fromarray(np.clip(field * 255.0 + 0.5, 0, 255).astype(np.uint8), mode="RGB")
    image.save(path, optimize=True)

    return path


def Rgb(*channels):
    return np.stack(channels, axis=-1)


# --- the collection -------------------------------------------------------------------


def WearMaps(rng):
    """Threshold maps: the order in which a surface gives way as the wear amount rises."""
    # Deliberately the smoothest of the set: the wear amount walks it in a readable order
    # rather than dissolving the surface, which is what makes it the one to start from.
    even = Normalize(Fbm(rng, 6, 6, 3, gain=0.4))

    blotches = Contrast(Normalize(Fbm(rng, 8, 8, 3, gain=0.45)), 1.5)

    speckle = Normalize(Fbm(rng, 32, 32, 3, gain=0.65))

    # Stretched rather than rotated: rubbing follows the length of a part, and a mask that
    # can be turned by the transform's tiling cannot be turned by anything else.
    streaks = Normalize(Fbm(rng, 3, 48, 4))

    # Distance to the nearest scattered point, inverted so the points themselves are the
    # low thresholds: pitting starts at spots and grows outwards.
    pitting = Normalize(Cellular(rng, 96))

    return {
        "even": even,
        "blotches": blotches,
        "speckle": speckle,
        "streaks": streaks,
        "pitting": pitting,
    }


def NoiseMaps(rng):
    return {
        "fine": Normalize(Fbm(rng, 48, 48, 3, gain=0.6)),
        "medium": Normalize(Fbm(rng, 16, 16, 4)),
        "coarse": Normalize(Fbm(rng, 6, 6, 3)),
        "grain": Normalize(ValueNoise(rng, 256, 256)),
        "cellular": Normalize(Cellular(rng, 40)),
    }


def ScratchMaps(rng):
    """A light field cut by dark lines, so raised wear reaches the lines first."""
    field = 0.62 + 0.30 * Normalize(Fbm(rng, 8, 8, 3))

    everywhere = (0.0, np.pi * 2.0)
    aligned = (-0.12, 0.12)

    return {
        "fine_random": np.clip(field - 0.55 * Strokes(rng, 260, 150, 1, 0.6, everywhere), 0.0, 1.0),
        "coarse_random": np.clip(field - 0.60 * Strokes(rng, 70, 260, 3, 1.1, everywhere), 0.0, 1.0),
        "fine_aligned": np.clip(field - 0.55 * Strokes(rng, 320, 190, 1, 0.6, aligned), 0.0, 1.0),
        "coarse_aligned": np.clip(field - 0.60 * Strokes(rng, 90, 320, 3, 1.1, aligned), 0.0, 1.0),
    }


def DirtMaps(rng):
    grime = Contrast(Normalize(Fbm(rng, 5, 5, 5, gain=0.62)), 1.8)

    # Long in one axis and short in the other, with a soft cloud gating where a run starts:
    # dirt runs from somewhere rather than covering everything evenly.
    drips = Normalize(Fbm(rng, 64, 3, 3)) * (0.35 + 0.65 * Normalize(Fbm(rng, 4, 4, 2)))

    smudges = Normalize(Fbm(rng, 2, 2, 3, gain=0.4))

    return {
        "grime": grime,
        "drips": Normalize(drips),
        "smudges": smudges,
    }


def ReflectMasks(rng):
    """Where a surface catches light at all: white is polished, black is matt.

    Deliberately near binary. These answer a yes-or-no question - a grip does not shine and
    a slide does - and a soft gray everywhere would only dim the highlight uniformly, which
    is what having no mask already does.
    """
    polished = Threshold(Normalize(Fbm(rng, 3, 3, 3, gain=0.45)), 0.34, 0.09)

    patchy = Threshold(Normalize(Fbm(rng, 5, 5, 3, gain=0.45)), 0.58, 0.08)

    # Stretched along one axis: brushed metal reflects in bands across the grain.
    brushed = Threshold(Normalize(Fbm(rng, 3, 140, 3)), 0.52, 0.05)

    # Fine bright points on a dull field, which is what a worn clear coat leaves behind.
    speckle = Threshold(Normalize(Fbm(rng, 40, 40, 3, gain=0.6)), 0.63, 0.04)

    # Hard edged regions: parts that were finished separately meet at a line, not a fade.
    panels = CellularRegions(rng, 18)

    return {
        "polished": polished,
        "patchy": patchy,
        "brushed": brushed,
        "speckle": speckle,
        "panels": panels,
    }


def Surfaces(rng):
    """What a worn surface shows underneath. Color, unlike everything above."""
    brushed = Normalize(Fbm(rng, 3, 200, 3))
    grain = Normalize(ValueNoise(rng, 256, 256))

    bare = 0.52 + 0.16 * brushed + 0.04 * grain
    heavy = 0.46 + 0.30 * Normalize(Fbm(rng, 2, 320, 3)) + 0.03 * grain
    dark = 0.20 + 0.10 * Normalize(Fbm(rng, 6, 96, 3)) + 0.03 * grain
    warm = 0.46 + 0.24 * brushed + 0.03 * grain
    plastic = 0.07 + 0.05 * Normalize(Fbm(rng, 96, 96, 2))

    return {
        "metal_bare": Rgb(bare, bare, bare * 1.02),
        "metal_brushed": Rgb(heavy, heavy, heavy * 1.02),
        "steel_dark": Rgb(dark, dark, dark * 1.06),
        "brass": Rgb(warm * 1.35, warm * 1.02, warm * 0.42),
        "plastic_black": Rgb(plastic, plastic, plastic * 1.08),
    }


def SphereNormals(size=SIZE):
    """The sphere map's own geometry.

    The reachable part of the image is the inscribed disc, because the coordinates come
    from a normal dotted with two view vectors. Outside it the normal is clamped to the
    rim, which extends the edge outwards and keeps filtering at the silhouette from
    dragging in whatever the corners happen to hold.
    """
    axis = (np.arange(size) + 0.5) / size * 2.0 - 1.0
    nx = axis[None, :] * np.ones((size, 1))
    ny = -axis[:, None] * np.ones((1, size))

    radius = np.sqrt(nx * nx + ny * ny)
    scale = np.where(radius > 1.0, 1.0 / np.maximum(radius, 1e-6), 1.0)

    nx = nx * scale
    ny = ny * scale
    nz = np.sqrt(np.clip(1.0 - nx * nx - ny * ny, 0.0, 1.0))

    return nx, ny, nz


def LitMatcap(color, lights, shininess, ambient, rim_strength):
    nx, ny, nz = SphereNormals()

    diffuse = np.zeros_like(nz)
    specular = np.zeros_like(nz)

    for direction, weight in lights:
        lx, ly, lz = direction / np.linalg.norm(direction)

        lambert = np.clip(nx * lx + ny * ly + nz * lz, 0.0, 1.0)
        diffuse += weight * lambert

        # Blinn-Phong against a fixed view along z, which is what a sphere map assumes.
        hx, hy, hz = lx, ly, lz + 1.0
        length = np.sqrt(hx * hx + hy * hy + hz * hz)
        highlight = np.clip((nx * hx + ny * hy + nz * hz) / length, 0.0, 1.0)
        specular += weight * highlight ** shininess

    rim = rim_strength * (1.0 - nz) ** 3.0
    shade = np.clip(ambient + diffuse, 0.0, 1.6)

    return Rgb(
        color[0] * shade + specular + rim,
        color[1] * shade + specular + rim,
        color[2] * shade + specular + rim,
    )


def MirrorMatcap(sharpness, tint):
    """A reflection of a plain surround rather than a lit ball.

    Chrome takes its look from what it reflects, so shading a sphere cannot produce it: a
    horizon and a light are what make the difference between chrome and gray paint.
    """
    nx, ny, nz = SphereNormals()

    # Reflect the view direction about the normal.
    rx = 2.0 * nz * nx
    ry = 2.0 * nz * ny
    rz = 2.0 * nz * nz - 1.0

    # Split along the reflected up axis rather than along depth: a split by depth is a
    # radial gradient, and a radial gradient is what gray paint looks like. The horizon has
    # to be a line for the surface to read as reflecting anything at all.
    sky = np.clip(ry * 2.5, -1.0, 1.0) * 0.5 + 0.5

    sun = np.clip(rx * -0.35 + ry * 0.80 + rz * 0.49, 0.0, 1.0) ** sharpness

    value = 0.14 + 0.66 * sky ** 1.4 + 0.08 * (1.0 - sky) + sun

    return Rgb(value * tint[0], value * tint[1], value * tint[2])


def Matcaps():
    return {
        "metal_soft": LitMatcap((0.44, 0.45, 0.48), [(np.array([-0.4, 0.6, 0.7]), 0.75)], 12.0, 0.20, 0.10),
        "metal_sharp": LitMatcap((0.36, 0.37, 0.40), [(np.array([-0.4, 0.6, 0.7]), 0.70), (np.array([0.6, -0.3, 0.5]), 0.25)], 64.0, 0.16, 0.16),
        "gunmetal": LitMatcap((0.22, 0.23, 0.26), [(np.array([-0.3, 0.7, 0.6]), 0.60)], 30.0, 0.12, 0.22),
        "lacquer": LitMatcap((0.30, 0.31, 0.34), [(np.array([-0.5, 0.5, 0.7]), 0.55)], 180.0, 0.10, 0.06),
        "chrome": MirrorMatcap(48.0, (1.0, 1.0, 1.02)),
        "chrome_soft": MirrorMatcap(8.0, (1.0, 0.99, 0.96)),
    }


# Seeded per folder rather than once for the whole run, so adding a folder later cannot
# shift the contents of the folders written before it.
FOLDERS = (
    ("masks/wear", 1, WearMaps, WriteGray),
    ("masks/noise", 2, NoiseMaps, WriteGray),
    ("masks/scratches", 3, ScratchMaps, WriteGray),
    ("masks/dirt", 4, DirtMaps, WriteGray),
    ("surfaces", 5, Surfaces, WriteRgb),
    ("masks/reflect", 6, ReflectMasks, WriteGray),
)


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=os.path.join("assets", "cstrike", "materials"))
    args = parser.parse_args()

    written = 0

    for folder, seed, maker, writer in FOLDERS:
        directory = os.path.join(args.out, *folder.split("/"))
        os.makedirs(directory, exist_ok=True)

        for name, field in maker(np.random.default_rng(seed)).items():
            print(f"wrote {writer(field, os.path.join(directory, name + '.png'))}")
            written += 1

    directory = os.path.join(args.out, "matcaps")
    os.makedirs(directory, exist_ok=True)

    for name, field in Matcaps().items():
        print(f"wrote {WriteRgb(field, os.path.join(directory, name + '.png'))}")
        written += 1

    print(f"{written} files")


if __name__ == "__main__":
    Main()
