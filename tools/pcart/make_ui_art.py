#!/usr/bin/env python3
"""Draw the PC interface art: keyboard keycaps, mouse buttons and arrows.

    python tools/pcart/make_ui_art.py

Writes resources/pc/ui/keycap.png, mouse.png and arrows.png. The art is drawn
at one texel per pixel and the game scales it by whole numbers, so it stays as
sharp as the rest of the interface. Key labels are not part of the art: the
game draws them onto the keycap in the Mojangles font.
"""
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "resources", "pc", "ui")

PALETTE = {
    ".": (0, 0, 0, 0),
    "K": (30, 30, 30, 255),
    "W": (255, 255, 255, 255),
    "F": (214, 214, 214, 255),
    "S": (168, 168, 168, 255),
    "D": (122, 122, 122, 255),
    "A": (255, 216, 64, 255),
    "B": (176, 140, 30, 255),
    "M": (56, 56, 56, 255),
}

# A key seen from the front: a light face lit from the top left, and the side of
# the key below it. Columns 0-3 and 5-8 are the edges; column 4 repeats to make
# the key as wide as its label. The label sits on rows 3-9.
KEYCAP = [
    ".KKKKKKK.",
    "KWWWWWWSK",
    "KWFFFFFSK",
    "KWFFFFFSK",
    "KWFFFFFSK",
    "KWFFFFFSK",
    "KWFFFFFSK",
    "KWFFFFFSK",
    "KWFFFFFSK",
    "KWFFFFFSK",
    "KWFFFFFSK",
    "KSSSSSSSK",
    "KDDDDDDDK",
    ".KKKKKKK.",
]

# A mouse, 11 by 14, with its two buttons, the wheel between them and the body.
MOUSE = [
    "...KKKKK...",
    "..KFFKFFK..",
    ".KWFFKFFSK.",
    ".KWFFMFFSK.",
    ".KWFFMFFSK.",
    ".KWFFKFFSK.",
    ".KKKKKKKKK.",
    ".KWFFFFFSK.",
    ".KWFFFFFSK.",
    ".KWFFFFFSK.",
    ".KWFFFFFSK.",
    ".KSSSSSSSK.",
    "..KDDDDDK..",
    "...KKKKK...",
]
LEFT_BUTTON = [(3, 1), (4, 1), (2, 2), (3, 2), (4, 2), (2, 3), (3, 3), (4, 3), (2, 4), (3, 4), (4, 4),
               (2, 5), (3, 5), (4, 5)]
RIGHT_BUTTON = [(6, 1), (7, 1), (6, 2), (7, 2), (8, 2), (6, 3), (7, 3), (8, 3), (6, 4), (7, 4), (8, 4),
                (6, 5), (7, 5), (8, 5)]
WHEEL = [(5, 3), (5, 4)]

# Arrows, 7 by 7, drawn in the label colour.
ARROW_UP = [
    "...#...",
    "..###..",
    ".#####.",
    "#######",
    "..###..",
    "..###..",
    "..###..",
]


def rotate(rows):
    size = len(rows)
    return ["".join(rows[size - 1 - x][y] for x in range(size)) for y in range(size)]


def write_png(path, width, height, pixels):
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for x in range(width):
            raw.extend(pixels[y * width + x])

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    data += chunk(b"IEND", b"")
    with open(path, "wb") as handle:
        handle.write(data)


def paint(rows):
    return [PALETTE[c] for row in rows for c in row]


def mouse_icon(highlight, colour):
    rows = [list(row) for row in MOUSE]
    for x, y in highlight:
        rows[y][x] = colour
    return ["".join(row) for row in rows]


def wheel_icon(direction):
    rows = [list(row) for row in mouse_icon(WHEEL, "A")]
    tip = 0 if direction < 0 else 13
    for x in (4, 5, 6):
        rows[tip][x] = "B"
    rows[tip - direction][5] = "B"
    return ["".join(row) for row in rows]


def main():
    os.makedirs(OUT, exist_ok=True)
    write_png(os.path.join(OUT, "keycap.png"), len(KEYCAP[0]), len(KEYCAP), paint(KEYCAP))

    icons = [mouse_icon(LEFT_BUTTON, "A"), mouse_icon(RIGHT_BUTTON, "A"), mouse_icon(WHEEL, "A"),
             wheel_icon(-1), wheel_icon(1)]
    width = len(MOUSE[0])
    sheet = []
    for y in range(len(MOUSE)):
        for icon in icons:
            sheet.extend(PALETTE[c] for c in icon[y])
    write_png(os.path.join(OUT, "mouse.png"), width * len(icons), len(MOUSE), sheet)

    arrows = [ARROW_UP]
    for _ in range(3):
        arrows.append(rotate(arrows[-1]))
    order = [arrows[0], arrows[2], arrows[3], arrows[1]]
    white = (255, 255, 255, 255)
    clear = (0, 0, 0, 0)
    pixels = []
    for y in range(7):
        for arrow in order:
            pixels.extend(white if c == "#" else clear for c in arrow[y])
    write_png(os.path.join(OUT, "arrows.png"), 7 * len(order), 7, pixels)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
