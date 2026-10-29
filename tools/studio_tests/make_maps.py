#!/usr/bin/env python3
"""Generate the .map source for the studio render test level.

One level, two rooms joined by a corridor. The corridor is there so vis gives the rooms
separate PVS: standing in one, the models of the other are not submitted at all, and the
overlay numbers therefore describe the room you are looking at rather than the whole
level.

  south room - one cycler per texture flag variant, in a row, plus two entities with a
               render mode other than normal and two carrying a glow shell, which are the
               passes that differ from an ordinary draw.
  north room - a grid of two heavy custom models repeated. Two rather than many:
               repeating one model is what batching across entities would target, and a
               single model alone would hide the cost of switching between them.

Usage:
    python make_maps.py OUTDIR --flag-models DIR --perf-models DIR --wad PATH
                        [--perf-a NAME] [--perf-b NAME] [--perf-count N]
"""

import argparse
import glob
import os
import struct

WALL_TEXTURE = "C1A0A_W3"
FLOOR_TEXTURE = "GENERIC114"

# cycler places the model at the entity origin, and player models are built around their
# centre rather than their feet, so anything at floor level ends up half sunk.
MODEL_Z = 40

# Signs behind and above the models, 128x32 to match the label textures one to one.
LABEL_WIDTH = 128
LABEL_HEIGHT = 32
LABEL_Y = 80
LABEL_TOP = 152

THICKNESS = 32

STUDIO_VERSION = 10

# kRenderFxGlowShell, counted out of the SDK's const.h; the map format carries the number.
GLOW_SHELL_FX = 19

# An unlit box inside the south room, entered through one low doorway. Anything a light
# adds to a model is invisible in a room lit as evenly as this level is: a muzzle flash
# contributes a few percent of what is already there. The alcove is where the lighting
# that arrives with an entity - its own lights now, its shadow later - can be seen at all.
# It sits inside the room's shell rather than beyond a corridor of its own, so it cannot
# open a leak; what light reaches it is the little that bounces through the doorway.
ALCOVE = {"x1": -848, "y1": -1328, "x2": -320, "y2": -820}
ALCOVE_DOOR = (-1184, -1056)
ALCOVE_DOOR_HEIGHT = 160

# Far enough apart that the shadow of one cannot reach the other.
ALCOVE_MODELS = (-680, -430)

# South room: the flag variants. The row has to fit across the screen at the default
# field of view, which at 90 degrees means standing back about as far as the row is wide.
SOUTH = {"x1": -880, "y1": -1360, "x2": 880, "y2": 400, "height": 512}
ROW_WIDTH = 1280
SPAWN_Y = -1120

# Corridor: an S, not a straight run. A straight corridor leaves the two rooms mutually
# visible however narrow it is, and vis then puts everything in one PVS: the first
# attempt compiled to four leaves with "average leafs visible: 3". Two turns remove the
# line of sight, which is what actually separates them.
CORRIDOR_HEIGHT = 192
SEP_Y1 = SOUTH["y2"]
LEG_A_X = (-496, -304)          # leaves the south room here
LEG_C_X = (304, 496)            # enters the north room here
CROSS_Y = (SEP_Y1 + 224, SEP_Y1 + 416)
SEP_Y2 = SEP_Y1 + 640

# North room: the grid. Wide and shallow so every model is inside the frustum at once,
# otherwise the frame is not measuring what it looks like it is measuring.
NORTH = {"x1": -1080, "y1": SEP_Y2, "x2": 1080, "y2": 2960, "height": 640}
GRID_FIRST_Y = NORTH["y1"] + 820
GRID_COLUMNS = 12
GRID_STEP = 130
GRID_FIRST_Y = NORTH["y1"] + 820

AXES = {
    "z": ("[ 1 0 0 0 ]", "[ 0 -1 0 0 ]"),
    "x": ("[ 0 1 0 0 ]", "[ 0 0 -1 0 ]"),
    "y": ("[ 1 0 0 0 ]", "[ 0 0 -1 0 ]"),
}


def Brush(x1, y1, z1, x2, y2, z2, texture=WALL_TEXTURE, front=None):
    # Three points per plane, wound so the normal points out of the brush. Get this
    # backwards and the half spaces no longer close, which the compiler reports as a
    # brush outside the world rather than as bad winding.
    faces = [
        ((x1, y2, z2), (x2, y2, z2), (x2, y1, z2), "z"),
        ((x1, y1, z1), (x2, y1, z1), (x2, y2, z1), "z"),
        ((x1, y2, z2), (x1, y1, z2), (x1, y1, z1), "x"),
        ((x2, y2, z1), (x2, y1, z1), (x2, y1, z2), "x"),
        ((x1, y1, z2), (x2, y1, z2), (x2, y1, z1), "y"),
        ((x2, y2, z2), (x1, y2, z2), (x1, y2, z1), "y"),
    ]

    lines = ["{"]
    for index, (a, b, c, axis) in enumerate(faces):
        u, v = AXES[axis]
        face_texture = texture

        # Face 4 is the one pointing back at the player; a sign puts its label there,
        # aligned so the 128x32 texture lands on the face exactly once.
        if front is not None and index == 4:
            face_texture = front
            u = f"[ 1 0 0 {-x1} ]"
            v = f"[ 0 0 -1 {z2} ]"

        lines.append(
            f"( {a[0]} {a[1]} {a[2]} ) ( {b[0]} {b[1]} {b[2]} ) ( {c[0]} {c[1]} {c[2]} ) "
            f"{face_texture} {u} {v} 0 1 1"
        )
    lines.append("}")

    return lines


def Geometry():
    """Two sealed rooms joined by an S shaped corridor.

    The rooms are different widths, so the block between them spans the wider extents and
    overlaps both shells. Sealing by overlap rather than by butting brushes together is
    what keeps the corners closed; butting them left the corner strips where the narrow
    room ends and the wide one has not started yet, and the compiler called it a leak.
    """
    t = THICKNESS
    lines = []

    outer_x1 = min(SOUTH["x1"], NORTH["x1"]) - t
    outer_x2 = max(SOUTH["x2"], NORTH["x2"]) + t
    top = NORTH["height"] + t

    # One slab under the whole level; separate floors per room would only add seams.
    lines += Brush(outer_x1, SOUTH["y1"] - t, -t, outer_x2, NORTH["y2"] + t, 0, FLOOR_TEXTURE)

    # South room shell, open towards the corridor.
    lines += Brush(SOUTH["x1"] - t, SOUTH["y1"] - t, SOUTH["height"], SOUTH["x2"] + t, SOUTH["y2"], SOUTH["height"] + t)
    lines += Brush(SOUTH["x1"] - t, SOUTH["y1"] - t, 0, SOUTH["x1"], SOUTH["y2"], SOUTH["height"] + t)
    lines += Brush(SOUTH["x2"], SOUTH["y1"] - t, 0, SOUTH["x2"] + t, SOUTH["y2"], SOUTH["height"] + t)
    lines += Brush(SOUTH["x1"] - t, SOUTH["y1"] - t, 0, SOUTH["x2"] + t, SOUTH["y1"], SOUTH["height"] + t)

    # The separator, built as three bands across Y with the corridor left as the gap.
    lines += Brush(outer_x1, SEP_Y1, 0, LEG_A_X[0], CROSS_Y[0], top)
    lines += Brush(LEG_A_X[1], SEP_Y1, 0, outer_x2, CROSS_Y[0], top)

    lines += Brush(outer_x1, CROSS_Y[0], 0, LEG_A_X[0], CROSS_Y[1], top)
    lines += Brush(LEG_C_X[1], CROSS_Y[0], 0, outer_x2, CROSS_Y[1], top)

    lines += Brush(outer_x1, CROSS_Y[1], 0, LEG_C_X[0], SEP_Y2, top)
    lines += Brush(LEG_C_X[1], CROSS_Y[1], 0, outer_x2, SEP_Y2, top)

    # Lid over the whole corridor, so it stays a low passage rather than a slot open to
    # both rooms' ceilings.
    lines += Brush(outer_x1, SEP_Y1, CORRIDOR_HEIGHT, outer_x2, SEP_Y2, top)

    # The dark alcove, four walls to the ceiling with a doorway in the east one.
    a = ALCOVE
    lines += Brush(a["x1"] - t, a["y2"], 0, a["x2"] + t, a["y2"] + t, SOUTH["height"])
    lines += Brush(a["x1"] - t, a["y1"] - t, 0, a["x2"] + t, a["y1"], SOUTH["height"])
    lines += Brush(a["x1"] - t, a["y1"] - t, 0, a["x1"], a["y2"] + t, SOUTH["height"])
    lines += Brush(a["x2"], a["y1"] - t, 0, a["x2"] + t, ALCOVE_DOOR[0], SOUTH["height"])
    lines += Brush(a["x2"], ALCOVE_DOOR[1], 0, a["x2"] + t, a["y2"] + t, SOUTH["height"])
    lines += Brush(a["x2"], ALCOVE_DOOR[0], ALCOVE_DOOR_HEIGHT, a["x2"] + t, ALCOVE_DOOR[1], SOUTH["height"])

    # North room shell, open towards the corridor.
    lines += Brush(NORTH["x1"] - t, NORTH["y1"], NORTH["height"], NORTH["x2"] + t, NORTH["y2"] + t, NORTH["height"] + t)
    lines += Brush(NORTH["x1"] - t, NORTH["y1"], 0, NORTH["x1"], NORTH["y2"] + t, NORTH["height"] + t)
    lines += Brush(NORTH["x2"], NORTH["y1"], 0, NORTH["x2"] + t, NORTH["y2"] + t, NORTH["height"] + t)
    lines += Brush(NORTH["x1"] - t, NORTH["y2"], 0, NORTH["x2"] + t, NORTH["y2"] + t, NORTH["height"] + t)

    return lines


def Entity(classname, **keys):
    lines = ["{", f'"classname" "{classname}"']
    for key, value in keys.items():
        lines.append(f'"{key}" "{value}"')
    lines.append("}")

    return lines


def IsTextureCompanion(path):
    """True for the <name>T.mdl that holds a model's textures.

    Matching on a "t.mdl" suffix alone is wrong and quietly drops real models: names
    like gign_fullbright.mdl or gign_chrome_flat.mdl end in exactly those characters.
    A companion is only a companion when the model it belongs to is there too.
    """
    stem, extension = os.path.splitext(path)

    if extension.lower() != ".mdl" or not stem.endswith(("T", "t")):
        return False

    return os.path.exists(stem[:-1] + extension)


def Sign(center_x, label):
    half = LABEL_WIDTH // 2

    return Brush(
        center_x - half, LABEL_Y, LABEL_TOP - LABEL_HEIGHT,
        center_x + half, LABEL_Y + 8, LABEL_TOP,
        WALL_TEXTURE,
        front="lbl_" + label.lower(),
    )


def StandingHeight(path):
    """How high to put a model's origin so that it rests on the floor.

    A cycler puts the model at the entity origin, and where that sits inside the model is
    the model's own business: the capture cube this started with hangs 129 units below its
    origin and 1 above it, so the fixed height meant for player models buried it. The
    header's own bounding box is zero in much of the installed content, but the one on the
    sequence is filled in.
    """
    if path is None:
        return MODEL_Z

    with open(path, "rb") as handle:
        data = handle.read()

    # Checked before the offsets are trusted, the same way mdl_flags.py checks the file it
    # rewrites: without this a truncated download aborts the whole generation on a struct
    # error, and a file that is not a studio model plants the entity at a garbage height.
    if len(data) < 212 or data[:4] != b"IDST":
        raise SystemExit(f"{path} is not a studio model")

    version = struct.unpack_from("<i", data, 4)[0]
    if version != STUDIO_VERSION:
        raise SystemExit(f"{path}: unsupported studio version {version}")

    numseq, seqindex = struct.unpack_from("<ii", data, 164)

    if numseq <= 0 or not (0 <= seqindex and seqindex + 108 <= len(data)):
        return MODEL_Z

    # mstudioseqdesc_t: bbmin lands 96 bytes in, and its z another 8.
    bbmin_z = struct.unpack_from("<f", data, seqindex + 96 + 8)[0]

    return int(round(-bbmin_z))


def FlagEntities(flag_dir, remap_model, remap_model_file):
    models = [m for m in sorted(glob.glob(os.path.join(flag_dir, "*.mdl"))) if not IsTextureCompanion(m)]

    if not models:
        raise SystemExit(f"no models in {flag_dir}; run mdl_flags.py first")

    # Not a flag variant and so not part of the row: a flag-free copy under its own name,
    # which is what the glow shell entities sit on. The plain variant carries the material
    # sample, and a material is drawn only by the shader path, so a shell judged against
    # the original path on that model would be judged through a red pattern.
    shells = [m for m in models if os.path.basename(m).endswith("_shell.mdl")]
    models = [m for m in models if m not in shells]

    if not shells:
        raise SystemExit(f"no <name>_shell.mdl in {flag_dir}; re-run mdl_flags.py, it now writes one")

    entities = []
    signs = []
    spacing = ROW_WIDTH // (len(models) + 1)
    x = -ROW_WIDTH // 2 + spacing
    parent = os.path.dirname(flag_dir)

    for path in models:
        relative = "models/" + os.path.relpath(path, parent).replace("\\", "/")
        entities.append(Entity("cycler", model=relative, origin=f"{x} 0 {MODEL_Z}", angles="0 270 0"))

        # gign_chrome_flat -> chromeflat, matching the texture the label wad carries.
        label = os.path.splitext(os.path.basename(path))[0].split("_", 1)[1].replace("_", "")
        signs += Sign(x, label)

        x += spacing

    # Behind the row so it does not hide it.
    first = "models/" + os.path.relpath(models[0], parent).replace("\\", "/")
    entities.append(Entity("cycler", model=first, origin=f"-240 320 {MODEL_Z}", angles="0 270 0", rendermode="4", renderamt="128"))
    entities.append(Entity("cycler", model=first, origin=f"240 320 {MODEL_Z}", angles="0 270 0", rendermode="5", renderamt="200"))

    # Render mode 1, which the engine draws as plain alpha blending: its own branch in
    # GL_SetRenderMode asks for a texture environment mode that is not one, so the driver
    # refuses it and nothing about the mode survives. Here to keep that reading honest.
    entities.append(Entity("cycler", model=first, origin=f"0 320 {MODEL_Z}", angles="0 270 0", rendermode="1", renderamt="150"))

    # A glow shell is a second pass over the same model with chrome forced onto every mesh,
    # and it takes its color from rendercolor rather than from the lighting. Two of them in
    # different colors: with one, a shell drawn in some color and a shell drawn in the
    # right color look the same.
    shell_relative = "models/" + os.path.relpath(shells[0], parent).replace("\\", "/")

    # In the dark, on the material free variant for the same reason the shells are.
    for x in ALCOVE_MODELS:
        entities.append(Entity(
            "cycler", model=shell_relative, origin=f"{x} -1150 {MODEL_Z}", angles="0 0 0",
        ))

    entities.append(Entity(
        "cycler", model=shell_relative, origin=f"-560 320 {MODEL_Z}", angles="0 270 0",
        renderfx=GLOW_SHELL_FX, rendercolor="255 0 0", renderamt="100",
    ))
    entities.append(Entity(
        "cycler", model=shell_relative, origin=f"560 320 {MODEL_Z}", angles="0 270 0",
        renderfx=GLOW_SHELL_FX, rendercolor="0 128 255", renderamt="100",
    ))

    # A skin whose name marks it for remapping resolves to a texture built per entity out
    # of the model's palette, not to the one the model carries, and only the engine's own
    # skin setup knows that. The sample is installed content rather than something written
    # here, and it is chosen by how much of it would change: nine tenths of its skin uses
    # saturated colors, so replacing their hue is unmistakable. A model whose remapped
    # texture is mostly gray proves nothing, whatever its name says.
    entities.append(Entity(
        "cycler",
        model=remap_model,
        origin=f"800 320 {StandingHeight(remap_model_file)}",
        angles="0 270 0",
    ))

    for center, label in (
        (-560, "shell_red"), (-240, "glow_rm4"), (0, "color_rm1"),
        (240, "add_rm5"), (560, "shell_blue"), (800, "remap"),
    ):
        signs += Brush(
            center - LABEL_WIDTH // 2, 400 - 40, LABEL_TOP - LABEL_HEIGHT,
            center + LABEL_WIDTH // 2, 400 - 32, LABEL_TOP,
            WALL_TEXTURE,
            front="lbl_" + label,
        )

    print(
        f"  south room: {len(models)} flag variants plus {len(entities) - len(models)} other entities, "
        f"{len(signs) // 8} signs"
    )

    return entities, signs


def PerfEntities(perf_dir, name_a, name_b, count):
    chosen = []
    for name in (name_a, name_b):
        path = os.path.join(perf_dir, name, name + ".mdl")
        if not os.path.exists(path):
            raise SystemExit(f"missing perf model: {path}")
        chosen.append(f"models/player/{name}/{name}.mdl")

    entities = []
    rows = (count + GRID_COLUMNS - 1) // GRID_COLUMNS

    for index in range(count):
        column = index % GRID_COLUMNS
        row = index // GRID_COLUMNS
        x = int((column - (GRID_COLUMNS - 1) / 2) * GRID_STEP)
        y = GRID_FIRST_Y + row * GRID_STEP

        # Alternate by column, so both models are on screen wherever you look.
        entities.append(Entity("cycler", model=chosen[column % 2], origin=f"{x} {y} {MODEL_Z}", angles="0 270 0"))

    print(f"  north room: {count} entities of {name_a} and {name_b} in {rows} rows")

    return entities


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument("outdir")
    parser.add_argument("--flag-models", required=True)
    parser.add_argument("--perf-models", required=True)
    parser.add_argument("--perf-a", default="YoRHa_2B")
    parser.add_argument("--perf-b", default="svinorez_t_robb")
    parser.add_argument("--perf-count", type=int, default=48)
    parser.add_argument("--remap-model", default="models/next21_adventures/treasure.mdl")
    parser.add_argument("--remap-model-file", default=None, help="the same model on disk, read for where it stands")
    parser.add_argument("--wad", required=True)
    parser.add_argument("--label-wad", required=True)
    args = parser.parse_args()

    os.makedirs(args.outdir, exist_ok=True)

    entities, signs = FlagEntities(args.flag_models, args.remap_model, args.remap_model_file)
    entities += PerfEntities(args.perf_models, args.perf_a, args.perf_b, args.perf_count)

    entities.append(Entity("info_player_start", origin=f"0 {SPAWN_Y} 40", angles="0 90 0"))

    # Spawning on this map hands out nothing, and the attached weapon model of a player is
    # a case of its own for the shader path, reachable only by carrying one. A pickup in
    # front of the spawn covers it without cheats. item 4 is the AK-47.
    entities.append(Entity("armoury_entity", origin=f"0 {SPAWN_Y + 96} 8", angles="0 90 0", item="4", count="1"))

    entities.append(Entity("light", origin="0 0 400", _light="255 255 255 400", _fade="0.4"))
    entities.append(Entity("light", origin=f"0 {SPAWN_Y // 2} 400", _light="255 255 255 300", _fade="0.5"))
    entities.append(Entity("light", origin=f"-400 {CROSS_Y[0]} 140", _light="255 255 255 200", _fade="0.8"))
    entities.append(Entity("light", origin=f"0 {GRID_FIRST_Y} 500", _light="255 255 255 500", _fade="0.3"))
    entities.append(Entity("light", origin=f"0 {NORTH['y1'] + 300} 500", _light="255 255 255 400", _fade="0.4"))

    lines = [
        "{",
        '"classname" "worldspawn"',
        # The texture axes are Valve 220; without this key the compilers parse the faces
        # as the older Quake format and choke on the first bracket.
        '"mapversion" "220"',
        f'"wad" "{args.wad};{args.label_wad}"',
        '"MaxRange" "8192"',
    ]
    lines += Geometry()
    lines += signs
    lines.append("}")

    for e in entities:
        lines += e

    path = os.path.join(args.outdir, "studio_test.map")
    open(path, "w", encoding="latin1").write("\n".join(lines) + "\n")
    print(f"wrote {path}")


if __name__ == "__main__":
    Main()
