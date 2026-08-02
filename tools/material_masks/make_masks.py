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


def smoothstep(t):
    return t * t * (3.0 - 2.0 * t)


def quintic(t):
    """Interpolation with a zero second derivative at the lattice points.

    Cubic leaves a crease along every lattice line, and on the coarse fields the collection
    leans on most that crease reads as a grid of squares rather than as a surface.
    """
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)


def value_noise(rng, period_x, period_y, size=SIZE):
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

    tx = quintic(fx - np.floor(fx))[None, :]
    ty = quintic(fy - np.floor(fy))[:, None]

    top = lattice[np.ix_(y0, x0)] * (1.0 - tx) + lattice[np.ix_(y0, x1)] * tx
    bottom = lattice[np.ix_(y1, x0)] * (1.0 - tx) + lattice[np.ix_(y1, x1)] * tx

    return top * (1.0 - ty) + bottom * ty


def fbm(rng, period_x, period_y, octaves, gain=0.5, size=SIZE):
    total = np.zeros((size, size))
    amplitude = 1.0
    weight = 0.0

    for octave in range(octaves):
        total += amplitude * value_noise(rng, period_x << octave, period_y << octave, size)
        weight += amplitude
        amplitude *= gain

    return total / weight


def cellular(rng, cells, size=SIZE):
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


def cellular_regions(rng, cells, size=SIZE):
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


def normalise(field):
    low = field.min()
    high = field.max()

    return (field - low) / (high - low) if high > low else np.zeros_like(field)


def contrast(field, amount):
    """Pushes a field away from its middle, so a smooth cloud reads as separate patches."""
    return np.clip((field - 0.5) * amount + 0.5, 0.0, 1.0)


def threshold(field, level, softness):
    """Cuts a field into near black and near white with a soft edge between.

    A mask that decides whether something happens at all has to actually decide: one that
    hovers around a single grey only scales the effect evenly everywhere, which is
    indistinguishable from having no mask.
    """
    return smoothstep(np.clip((field - (level - softness)) / (2.0 * softness), 0.0, 1.0))


def strokes(rng, count, length, width, blur, angle_range, size=SIZE):
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


def write_grey(field, path):
    image = Image.fromarray(np.clip(field * 255.0 + 0.5, 0, 255).astype(np.uint8), mode="L")
    image.save(path, optimize=True)

    return path


def write_rgb(field, path):
    image = Image.fromarray(np.clip(field * 255.0 + 0.5, 0, 255).astype(np.uint8), mode="RGB")
    image.save(path, optimize=True)

    return path


def rgb(*channels):
    return np.stack(channels, axis=-1)


# --- the collection -------------------------------------------------------------------


def wear_maps(rng):
    """Threshold maps: the order in which a surface gives way as the wear amount rises."""
    # Deliberately the smoothest of the set: the wear amount walks it in a readable order
    # rather than dissolving the surface, which is what makes it the one to start from.
    even = normalise(fbm(rng, 6, 6, 3, gain=0.4))

    blotches = contrast(normalise(fbm(rng, 8, 8, 3, gain=0.45)), 1.5)

    speckle = normalise(fbm(rng, 32, 32, 3, gain=0.65))

    # Stretched rather than rotated: rubbing follows the length of a part, and a mask that
    # can be turned by the transform's tiling cannot be turned by anything else.
    streaks = normalise(fbm(rng, 3, 48, 4))

    # Distance to the nearest scattered point, inverted so the points themselves are the
    # low thresholds: pitting starts at spots and grows outwards.
    pitting = normalise(cellular(rng, 96))

    return {
        "even": even,
        "blotches": blotches,
        "speckle": speckle,
        "streaks": streaks,
        "pitting": pitting,
    }


def noise_maps(rng):
    return {
        "fine": normalise(fbm(rng, 48, 48, 3, gain=0.6)),
        "medium": normalise(fbm(rng, 16, 16, 4)),
        "coarse": normalise(fbm(rng, 6, 6, 3)),
        "grain": normalise(value_noise(rng, 256, 256)),
        "cellular": normalise(cellular(rng, 40)),
    }


def scratch_maps(rng):
    """A light field cut by dark lines, so raised wear reaches the lines first."""
    field = 0.62 + 0.30 * normalise(fbm(rng, 8, 8, 3))

    everywhere = (0.0, np.pi * 2.0)
    aligned = (-0.12, 0.12)

    return {
        "fine_random": np.clip(field - 0.55 * strokes(rng, 260, 150, 1, 0.6, everywhere), 0.0, 1.0),
        "coarse_random": np.clip(field - 0.60 * strokes(rng, 70, 260, 3, 1.1, everywhere), 0.0, 1.0),
        "fine_aligned": np.clip(field - 0.55 * strokes(rng, 320, 190, 1, 0.6, aligned), 0.0, 1.0),
        "coarse_aligned": np.clip(field - 0.60 * strokes(rng, 90, 320, 3, 1.1, aligned), 0.0, 1.0),
    }


def dirt_maps(rng):
    grime = contrast(normalise(fbm(rng, 5, 5, 5, gain=0.62)), 1.8)

    # Long in one axis and short in the other, with a soft cloud gating where a run starts:
    # dirt runs from somewhere rather than covering everything evenly.
    drips = normalise(fbm(rng, 64, 3, 3)) * (0.35 + 0.65 * normalise(fbm(rng, 4, 4, 2)))

    smudges = normalise(fbm(rng, 2, 2, 3, gain=0.4))

    return {
        "grime": grime,
        "drips": normalise(drips),
        "smudges": smudges,
    }


def reflect_masks(rng):
    """Where a surface catches light at all: white is polished, black is matt.

    Deliberately near binary. These answer a yes-or-no question - a grip does not shine and
    a slide does - and a soft grey everywhere would only dim the highlight uniformly, which
    is what having no mask already does.
    """
    polished = threshold(normalise(fbm(rng, 3, 3, 3, gain=0.45)), 0.34, 0.09)

    patchy = threshold(normalise(fbm(rng, 5, 5, 3, gain=0.45)), 0.58, 0.08)

    # Stretched along one axis: brushed metal reflects in bands across the grain.
    brushed = threshold(normalise(fbm(rng, 3, 140, 3)), 0.52, 0.05)

    # Fine bright points on a dull field, which is what a worn clear coat leaves behind.
    speckle = threshold(normalise(fbm(rng, 40, 40, 3, gain=0.6)), 0.63, 0.04)

    # Hard edged regions: parts that were finished separately meet at a line, not a fade.
    panels = cellular_regions(rng, 18)

    return {
        "polished": polished,
        "patchy": patchy,
        "brushed": brushed,
        "speckle": speckle,
        "panels": panels,
    }


def surfaces(rng):
    """What a worn surface shows underneath. Colour, unlike everything above."""
    brushed = normalise(fbm(rng, 3, 200, 3))
    grain = normalise(value_noise(rng, 256, 256))

    bare = 0.52 + 0.16 * brushed + 0.04 * grain
    heavy = 0.46 + 0.30 * normalise(fbm(rng, 2, 320, 3)) + 0.03 * grain
    dark = 0.20 + 0.10 * normalise(fbm(rng, 6, 96, 3)) + 0.03 * grain
    warm = 0.46 + 0.24 * brushed + 0.03 * grain
    plastic = 0.07 + 0.05 * normalise(fbm(rng, 96, 96, 2))

    return {
        "metal_bare": rgb(bare, bare, bare * 1.02),
        "metal_brushed": rgb(heavy, heavy, heavy * 1.02),
        "steel_dark": rgb(dark, dark, dark * 1.06),
        "brass": rgb(warm * 1.35, warm * 1.02, warm * 0.42),
        "plastic_black": rgb(plastic, plastic, plastic * 1.08),
    }


def sphere_normals(size=SIZE):
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


def lit_matcap(colour, lights, shininess, ambient, rim_strength):
    nx, ny, nz = sphere_normals()

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

    return rgb(
        colour[0] * shade + specular + rim,
        colour[1] * shade + specular + rim,
        colour[2] * shade + specular + rim,
    )


def mirror_matcap(sharpness, tint):
    """A reflection of a plain surround rather than a lit ball.

    Chrome takes its look from what it reflects, so shading a sphere cannot produce it: a
    horizon and a light are what make the difference between chrome and grey paint.
    """
    nx, ny, nz = sphere_normals()

    # Reflect the view direction about the normal.
    rx = 2.0 * nz * nx
    ry = 2.0 * nz * ny
    rz = 2.0 * nz * nz - 1.0

    # Split along the reflected up axis rather than along depth: a split by depth is a
    # radial gradient, and a radial gradient is what grey paint looks like. The horizon has
    # to be a line for the surface to read as reflecting anything at all.
    sky = np.clip(ry * 2.5, -1.0, 1.0) * 0.5 + 0.5

    sun = np.clip(rx * -0.35 + ry * 0.80 + rz * 0.49, 0.0, 1.0) ** sharpness

    value = 0.14 + 0.66 * sky ** 1.4 + 0.08 * (1.0 - sky) + sun

    return rgb(value * tint[0], value * tint[1], value * tint[2])


def matcaps():
    return {
        "metal_soft": lit_matcap((0.44, 0.45, 0.48), [(np.array([-0.4, 0.6, 0.7]), 0.75)], 12.0, 0.20, 0.10),
        "metal_sharp": lit_matcap((0.36, 0.37, 0.40), [(np.array([-0.4, 0.6, 0.7]), 0.70), (np.array([0.6, -0.3, 0.5]), 0.25)], 64.0, 0.16, 0.16),
        "gunmetal": lit_matcap((0.22, 0.23, 0.26), [(np.array([-0.3, 0.7, 0.6]), 0.60)], 30.0, 0.12, 0.22),
        "lacquer": lit_matcap((0.30, 0.31, 0.34), [(np.array([-0.5, 0.5, 0.7]), 0.55)], 180.0, 0.10, 0.06),
        "chrome": mirror_matcap(48.0, (1.0, 1.0, 1.02)),
        "chrome_soft": mirror_matcap(8.0, (1.0, 0.99, 0.96)),
    }


# Seeded per folder rather than once for the whole run, so adding a folder later cannot
# shift the contents of the folders written before it.
FOLDERS = (
    ("masks/wear", 1, wear_maps, write_grey),
    ("masks/noise", 2, noise_maps, write_grey),
    ("masks/scratches", 3, scratch_maps, write_grey),
    ("masks/dirt", 4, dirt_maps, write_grey),
    ("surfaces", 5, surfaces, write_rgb),
    ("masks/reflect", 6, reflect_masks, write_grey),
)


def main():
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

    for name, field in matcaps().items():
        print(f"wrote {write_rgb(field, os.path.join(directory, name + '.png'))}")
        written += 1

    print(f"{written} files")


if __name__ == "__main__":
    main()
