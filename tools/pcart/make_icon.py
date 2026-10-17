#!/usr/bin/env python3
"""Draw the icons, from scratch: a crafting table for the game and a grass block for the launcher.

    python tools/pcart/make_icon.py

Writes:
  resources/pc/icon.png         256 by 256, the game's window icon
  resources/pc/icon.ico         16 to 256 pixels, the game executable's icon
  resources/launcher/icon.png   the same for the launcher
  resources/launcher/icon.ico

Each block's faces are 16 by 16 textures, drawn here or generated from a fixed
seed, mapped onto the three visible faces of an isometric cube. Each size is
drawn at four times its size and averaged down, so the edges stay smooth while
the texels stay square.
"""
import math
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_ui_art import png_bytes, write_png  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

TEXELS = 16
SEED = 20261017
SIZES = (16, 24, 32, 48, 64, 128, 256)
SUPERSAMPLE = 4

GRASS = [(84, 150, 48), (98, 166, 56), (112, 180, 64), (72, 134, 42), (126, 192, 76)]
GRASS_WEIGHTS = [5, 6, 4, 3, 1]
SOIL = [(128, 90, 60), (116, 80, 52), (142, 102, 70), (100, 70, 46), (154, 116, 82)]
SOIL_WEIGHTS = [6, 5, 3, 3, 1]

WOOD = {
    "W": (250, 214, 144),
    "L": (232, 184, 112),
    "P": (210, 158, 92),
    "Q": (190, 140, 80),
    "D": (166, 118, 64),
    "G": (96, 62, 30),
    "F": (92, 60, 30),
    "E": (68, 44, 20),
    "K": (44, 28, 12),
    "M": (236, 238, 242),
    "N": (156, 160, 168),
    "H": (190, 126, 66),
    "J": (146, 94, 46),
}

# The front: the tabletop's edge with dark corner caps and its shadow, corner
# posts, and a saw and a hammer hanging between them. "-" is the tabletop's
# planks, "|" the posts and "." the panel's boards.
TABLE_FRONT = [
    "KKWWWWWWWWWWWWKK",
    "KE------------EK",
    "KK------------KK",
    "KKKKKKKKKKKKKKKK",
    "||.JJJJ..MMMMM||",
    "||.J..J..MMMMM||",
    "||.JJJJ..NNN.N||",
    "||.MMMN....JJ.||",
    "||.MMMM....JJ.||",
    "||.MMMN....JJ.||",
    "||.MMMM....JJ.||",
    "||.MMMN....KK.||",
    "||.MMMM....JJ.||",
    "||.NNN.....KK.||",
    "||.N........N.||",
    "KKKKKKKKKKKKKKKK",
]

# The other side: an axe and a ruler.
TABLE_SIDE = [
    "KKWWWWWWWWWWWWKK",
    "KE------------EK",
    "KK------------KK",
    "KKKKKKKKKKKKKKKK",
    "||..JJMMMM.WW.||",
    "||..JJMMMMMWK.||",
    "||..JJMMMM.WW.||",
    "||..JJNN...WK.||",
    "||..JJ.....WW.||",
    "||..JJ.....WK.||",
    "||..JJ.....WW.||",
    "||..JJ.....WK.||",
    "||..KK.....WW.||",
    "||..KK.....WK.||",
    "||.N........N.||",
    "KKKKKKKKKKKKKKKK",
]

# "tilt" flattens the top to that height over its width and fills the square;
# without it the cube is drawn in true isometric.
TABLE_STYLE = {"top": 1.0, "left": 0.86, "right": 0.66, "edge": 1.10, "fall": 0.06, "tilt": 0.5}
GRASS_STYLE = {"top": 1.0, "left": 0.80, "right": 0.62, "edge": 1.0, "fall": 0.0}


def check_rows(rows):
    for row in rows:
        if len(row) != TEXELS:
            raise SystemExit("texture rows must be %d wide: %r" % (TEXELS, row))


def table_top(rng):
    lines = (0, 5, 10, 15)
    texture = []
    for y in range(TEXELS):
        row = []
        for x in range(TEXELS):
            if x in (0, 15) or y in (0, 15):
                row.append(WOOD["F"])
            elif x in lines or y in lines:
                row.append(WOOD["G"])
            else:
                cellX = (x - 1) % 5
                cellY = (y - 1) % 5
                if cellX == 0 or cellY == 0:
                    row.append(WOOD["L"])
                elif cellX == 3 or cellY == 3:
                    row.append(WOOD["D"])
                elif rng.random() < 0.12:
                    row.append(WOOD["Q"])
                else:
                    row.append(WOOD["P"])
        texture.append(row)

    for corner_x, step_x in ((0, 1), (15, -1)):
        for corner_y, step_y in ((0, 1), (15, -1)):
            for offset in range(3):
                texture[corner_y][corner_x + step_x * offset] = WOOD["K"]
                texture[corner_y + step_y * offset][corner_x] = WOOD["K"]
            texture[corner_y + step_y][corner_x + step_x] = WOOD["E"]
    for edge in (5, 10):
        for x, y in ((edge, 0), (edge, 15), (0, edge), (15, edge)):
            texture[y][x] = WOOD["K"]
    return texture


def table_side(rows, rng):
    check_rows(rows)
    texture = []
    for y, row in enumerate(rows):
        line = []
        for x, mark in enumerate(row):
            grain = rng.random()
            if mark == "-":
                base = "L" if y == 1 else "P"
                line.append(WOOD["Q" if grain < 0.15 else base])
            elif mark == "|":
                line.append(WOOD["E" if (x % 2 == 1) != (grain < 0.15) else "F"])
            elif mark == ".":
                base = "P" if (x // 2) % 2 == 0 else "Q"
                line.append(WOOD["E" if grain < 0.08 else base])
            else:
                line.append(WOOD[mark])
        texture.append(line)
    return texture


def crafting_table():
    rng = random.Random(SEED)
    return table_top(rng), table_side(TABLE_FRONT, rng), table_side(TABLE_SIDE, rng)


def grass_block():
    rng = random.Random(SEED)
    top = [[rng.choices(GRASS, GRASS_WEIGHTS)[0] for _ in range(TEXELS)] for _ in range(TEXELS)]

    depth = []
    for x in range(TEXELS):
        step = rng.choice((-1, 0, 0, 1))
        previous = depth[-1] if depth else 3
        depth.append(max(2, min(5, previous + step)))
    for x in range(TEXELS):
        if rng.random() < 0.25:
            depth[x] += 1

    side = []
    for y in range(TEXELS):
        row = []
        for x in range(TEXELS):
            if y < depth[x]:
                row.append(rng.choices(GRASS, GRASS_WEIGHTS)[0])
            else:
                row.append(rng.choices(SOIL, SOIL_WEIGHTS)[0])
        side.append(row)
    return top, side, side


def texel(texture, u, v, light):
    x = min(TEXELS - 1, max(0, int(u * TEXELS)))
    y = min(TEXELS - 1, max(0, int(v * TEXELS)))
    red, green, blue = texture[y][x]
    return (min(255.0, red * light), min(255.0, green * light), min(255.0, blue * light))


def render(size, faces, style):
    top, left, right = faces
    front_edge = 1.0 - 1.0 / TEXELS
    canvas = size * SUPERSAMPLE
    margin = canvas * 0.04
    if "tilt" in style:
        half_width = (canvas - 2 * margin) / 2.0
        half_height = half_width * style["tilt"]
        edge = canvas - 2 * margin - 2 * half_height
    else:
        edge = (canvas - 2 * margin) / 2.0
        half_width = edge * math.cos(math.radians(30))
        half_height = edge * 0.5
    centre = canvas / 2.0
    peak = margin

    def colour(px, py):
        dx = px - centre
        dy = py - peak
        a = (dx / half_width + dy / half_height) / 2.0
        b = (dy / half_height - dx / half_width) / 2.0
        if 0.0 <= a < 1.0 and 0.0 <= b < 1.0:
            edge_light = style["edge"] if a >= front_edge or b >= front_edge else 1.0
            return texel(top, a, b, style["top"] * edge_light)

        dx = px - (centre - half_width)
        dy = py - (peak + half_height)
        a = dx / half_width
        if 0.0 <= a < 1.0:
            c = (dy - a * half_height) / edge
            if 0.0 <= c < 1.0:
                return texel(left, a, c, style["left"] * (1.0 - style["fall"] * c))

        dx = px - centre
        dy = py - (peak + 2 * half_height)
        a = dx / half_width
        if 0.0 <= a < 1.0:
            c = (dy + a * half_height) / edge
            if 0.0 <= c < 1.0:
                return texel(right, a, c, style["right"] * (1.0 - style["fall"] * c))
        return None

    pixels = []
    area = SUPERSAMPLE * SUPERSAMPLE
    for y in range(size):
        for x in range(size):
            red = green = blue = 0.0
            covered = 0
            for sy in range(SUPERSAMPLE):
                for sx in range(SUPERSAMPLE):
                    sample = colour(x * SUPERSAMPLE + sx + 0.5, y * SUPERSAMPLE + sy + 0.5)
                    if sample is not None:
                        red += sample[0]
                        green += sample[1]
                        blue += sample[2]
                        covered += 1
            if covered == 0:
                pixels.append((0, 0, 0, 0))
            else:
                pixels.append((int(round(red / covered)), int(round(green / covered)), int(round(blue / covered)),
                               int(round(255 * covered / area))))
    return pixels


def ico_bytes(images):
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = len(header) + 16 * len(images)
    entries = b""
    data = b""
    for size, png in images:
        dimension = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dimension, dimension, 0, 0, 1, 32, len(png), offset + len(data))
        data += png
    return header + entries + data


def write_icon(folder, faces, style):
    os.makedirs(folder, exist_ok=True)
    images = [(size, png_bytes(size, size, render(size, faces, style))) for size in SIZES]
    write_png(os.path.join(folder, "icon.png"), 256, 256, render(256, faces, style))
    with open(os.path.join(folder, "icon.ico"), "wb") as handle:
        handle.write(ico_bytes(images))
    print("wrote", folder)


def main():
    write_icon(os.path.join(ROOT, "resources", "pc"), crafting_table(), TABLE_STYLE)
    write_icon(os.path.join(ROOT, "resources", "launcher"), grass_block(), GRASS_STYLE)


if __name__ == "__main__":
    main()
