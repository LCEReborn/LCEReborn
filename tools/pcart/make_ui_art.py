#!/usr/bin/env python3
"""Draw the PC interface art: keycaps, mouse buttons and controller buttons.

    python tools/pcart/make_ui_art.py

Writes into resources/pc/ui:
  keycap.png    a keyboard key, its middle column repeated to fit the label
  mouse.png     a mouse with each button lit, the wheel turning, and at rest
  arrows.png    the arrow keys' labels
  pad_*.png     controller button shapes: round, stick, bumper, trigger, pill
                and d-pad, in four marker colours (outline, highlight, face and
                shadow) that the game replaces with each controller's colours
  symbols.png   9 by 9 button symbols: cross, circle, square, triangle, menu, view,
                plus and minus
  touch.png     the on-screen touch controls: 20 by 20 button frames (at rest,
                pressed and switched on) along the top, 12 by 12 icons below
                them in TOUCH_ICONS order, the 48 by 48 joystick pad, and
                beside it a white block and the digits 0 to 9

The art is drawn at one texel per pixel and the game scales it by whole
numbers, so it stays as sharp as the rest of the interface. Labels are not part
of the art: the game writes them in the Mojangles font.
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
    with open(path, "wb") as handle:
        handle.write(png_bytes(width, height, pixels))


def png_bytes(width, height, pixels):
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
    return data


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


# Marker colours for the controller shapes; the game swaps them for each
# controller's own colours.
MARK = {
    "K": (0, 0, 0, 255),
    "W": (255, 255, 255, 255),
    "F": (128, 128, 128, 255),
    "S": (64, 64, 64, 255),
    ".": (0, 0, 0, 0),
}

SYMBOLS = [
    ["#.......#", ".#.....#.", "..#...#..", "...#.#...", "....#....", "...#.#...", "..#...#..", ".#.....#.",
     "#.......#"],
    ["..#####..", ".#.....#.", "#.......#", "#.......#", "#.......#", "#.......#", "#.......#", ".#.....#.",
     "..#####.."],
    [".........", ".#######.", ".#.....#.", ".#.....#.", ".#.....#.", ".#.....#.", ".#.....#.", ".#######.",
     "........."],
    ["....#....", "...#.#...", "...#.#...", "..#...#..", "..#...#..", ".#.....#.", ".#.....#.", "#.......#",
     "#########"],
    [".........", ".........", ".#######.", ".........", ".#######.", ".........", ".#######.", ".........",
     "........."],
    [".........", "#####....", "#...#....", "#..#####.", "#..#.#.#.", "####.#.#.", "...#...#.", "...#####.",
     "........."],
    [".........", ".........", "....#....", "....#....", "..#####..", "....#....", "....#....", ".........",
     "........."],
    [".........", ".........", ".........", ".........", "..#####..", ".........", ".........", ".........",
     "........."],
]


def rounded_rect(width, height, top, bottom):
    def inside(x, y):
        px = x + 0.5
        py = y + 0.5
        radius = top if py < height / 2.0 else bottom
        cx = min(max(px, radius), width - radius)
        cy = min(max(py, radius), height - radius)
        return (px - cx) ** 2 + (py - cy) ** 2 <= radius * radius
    return inside


def circle(size):
    centre = size / 2.0
    radius = size / 2.0 - 0.1

    def inside(x, y):
        return (x + 0.5 - centre) ** 2 + (y + 0.5 - centre) ** 2 <= radius * radius
    return inside


def cross(size, arm):
    middle = size // 2

    def inside(x, y):
        return abs(x - middle) <= arm or abs(y - middle) <= arm
    return inside


def shade(width, height, inside, ring=None):
    rows = []
    for y in range(height):
        row = ""
        for x in range(width):
            if not inside(x, y):
                row += "."
                continue
            edge = any(not (0 <= x + dx < width and 0 <= y + dy < height and inside(x + dx, y + dy))
                       for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            if edge:
                row += "K"
            elif not inside(x - 1, y - 1) or not inside(x, y - 2):
                row += "W"
            elif not inside(x + 1, y + 1) or not inside(x, y + 2):
                row += "S"
            elif ring is not None and ring(x, y):
                row += "S"
            else:
                row += "F"
        rows.append(row)
    return rows


def stick_ring(size):
    centre = size / 2.0

    def ring(x, y):
        distance = ((x + 0.5 - centre) ** 2 + (y + 0.5 - centre) ** 2) ** 0.5
        return 3.5 <= distance <= 4.4
    return ring


PAD_SHAPES = {
    "round": (15, 15, circle(15), None),
    "stick": (15, 15, circle(15), stick_ring(15)),
    "bumper": (20, 11, rounded_rect(20, 11, 4.5, 2.0), None),
    "trigger": (17, 15, rounded_rect(17, 15, 7.0, 2.0), None),
    "pill": (17, 11, rounded_rect(17, 11, 5.5, 5.5), None),
    "dpad": (15, 15, cross(15, 2), None),
}


# Touch controls, drawn like the hotbar's slots: a dark outline, a bevelled grey
# frame and a see-through middle. Icons are white with a drop shadow, like the
# interface's text.
TOUCH_COLOURS = {
    ".": (0, 0, 0, 0),
    "K": (16, 16, 16, 220),
    "W": (236, 236, 236, 255),
    "F": (168, 168, 168, 255),
    "S": (88, 88, 88, 255),
    "I": (0, 0, 0, 104),
    "P": (255, 255, 255, 72),
    "L": (255, 224, 96, 120),
    "G": (0, 0, 0, 64),
}
TOUCH_BUTTON = 20
TOUCH_ICON = 12
TOUCH_PAD = 48

TOUCH_ICONS = [
    ("jump", [
        ".....##.....",
        "....####....",
        "...######...",
        "..########..",
        ".##########.",
        "....####....",
        "....####....",
        "....####....",
        "............",
        ".##########.",
        ".##########.",
        "............",
    ]),
    ("sneak", [
        "....####....",
        "....####....",
        "....####....",
        ".##########.",
        "..########..",
        "...######...",
        "....####....",
        ".....##.....",
        "............",
        ".##########.",
        ".##########.",
        "............",
    ]),
    ("fly_up", [
        ".....##.....",
        "....####....",
        "...##..##...",
        "..##....##..",
        ".##..##..##.",
        "....####....",
        "...##..##...",
        "..##....##..",
        ".##......##.",
        "............",
        ".##########.",
        "............",
    ]),
    ("fly_down", [
        ".##########.",
        "............",
        ".##......##.",
        "..##....##..",
        "...##..##...",
        "....####....",
        ".##..##..##.",
        "..##....##..",
        "...##..##...",
        "....####....",
        ".....##.....",
        "............",
    ]),
    ("swim_up", [
        ".....##.....",
        "....####....",
        "...######...",
        "..########..",
        "....####....",
        "....####....",
        "............",
        ".##...##....",
        "#..#.#..#.#.",
        "....#....#..",
        "............",
        "............",
    ]),
    ("swim_down", [
        ".##...##....",
        "#..#.#..#.#.",
        "....#....#..",
        "............",
        "....####....",
        "....####....",
        "..########..",
        "...######...",
        "....####....",
        ".....##.....",
        "............",
        "............",
    ]),
    ("dismount", [
        "#######.....",
        "#.....#.....",
        "#.....#..#..",
        "#.....#..##.",
        "#...#######.",
        "#...#######.",
        "#.....#..##.",
        "#.....#..#..",
        "#.....#.....",
        "#######.....",
        "............",
        "............",
    ]),
    ("attack", [
        ".........###",
        "........###.",
        ".......###..",
        "......###...",
        ".#...###....",
        ".##.###.....",
        "..####......",
        "...##.......",
        "..####......",
        ".##..##.....",
        "##..........",
        "............",
    ]),
    ("use", [
        ".....##.....",
        ".....##.....",
        "............",
        "............",
        "............",
        "##...##...##",
        "##...##...##",
        "............",
        "............",
        "............",
        ".....##.....",
        ".....##.....",
    ]),
    ("pause", [
        "............",
        "..###..###..",
        "..###..###..",
        "..###..###..",
        "..###..###..",
        "..###..###..",
        "..###..###..",
        "..###..###..",
        "..###..###..",
        "..###..###..",
        "............",
        "............",
    ]),
    ("chat", [
        "............",
        ".##########.",
        "#..........#",
        "#.##.##.##.#",
        "#.##.##.##.#",
        "#..........#",
        ".####.#####.",
        "....#.#.....",
        "....##......",
        "....#.......",
        "............",
        "............",
    ]),
    ("inventory", [
        "............",
        "............",
        "............",
        "............",
        "............",
        ".##..##..##.",
        ".##..##..##.",
        "............",
        "............",
        "............",
        "............",
        "............",
    ]),
    ("crafting", [
        "............",
        ".##..##..##.",
        ".##..##..##.",
        "............",
        "............",
        ".##..##..##.",
        ".##..##..##.",
        "............",
        "............",
        ".##..##..##.",
        ".##..##..##.",
        "............",
    ]),
    ("arrow_up", [
        "............",
        "............",
        "............",
        ".....##.....",
        "....####....",
        "...######...",
        "..########..",
        ".##########.",
        "............",
        "............",
        "............",
        "............",
    ]),
    ("arrow_up_left", [
        "............",
        ".#######....",
        ".######.....",
        ".#####......",
        ".####.......",
        ".###........",
        ".##.........",
        ".#..........",
        "............",
        "............",
        "............",
        "............",
    ]),
]


# Buttons added after the d-pad arrows, so the arrows keep their places: the
# sprint toggle, pick block, camera perspective, the interact button shown
# when the label cannot be written in the font, and the look prompt used in
# tutorial text.
TOUCH_EXTRA_ICONS = [
    ("sprint", [
        "............",
        "............",
        ".##...##....",
        "..##...##...",
        "...##...##..",
        "....##...##.",
        "....##...##.",
        "...##...##..",
        "..##...##...",
        ".##...##....",
        "............",
        "............",
    ]),
    ("pick_block", [
        "........#...",
        "........#...",
        "......#####.",
        "........#...",
        "........#...",
        "#######.....",
        "#.....#.....",
        "#.###.#.....",
        "#.###.#.....",
        "#.###.#.....",
        "#.....#.....",
        "#######.....",
    ]),
    ("perspective", [
        "............",
        "............",
        "....####....",
        "..##....##..",
        ".#...##...#.",
        "#...####...#",
        "#...####...#",
        ".#...##...#.",
        "..##....##..",
        "....####....",
        "............",
        "............",
    ]),
    ("interact", [
        "............",
        ".....##.....",
        ".....##.....",
        ".....##.....",
        "............",
        ".###....###.",
        ".###....###.",
        "............",
        ".....##.....",
        ".....##.....",
        ".....##.....",
        "............",
    ]),
    ("look", [
        ".....##.....",
        "....####....",
        "...######...",
        ".....##.....",
        "..#..##..#..",
        ".##########.",
        ".##########.",
        "..#..##..#..",
        ".....##.....",
        "...######...",
        "....####....",
        ".....##.....",
    ]),
]


# Digits for the amount a held finger is taking, 3 by 5, drawn after a 4 by 4
# white block that the bars are filled from.
TOUCH_DIGITS = [
    ["###", "#.#", "#.#", "#.#", "###"],
    [".#.", "##.", ".#.", ".#.", "###"],
    ["###", "..#", "###", "#..", "###"],
    ["###", "..#", ".##", "..#", "###"],
    ["#.#", "#.#", "###", "..#", "..#"],
    ["###", "#..", "###", "..#", "###"],
    ["###", "#..", "###", "#.#", "###"],
    ["###", "..#", ".#.", ".#.", ".#."],
    ["###", "#.#", "###", "#.#", "###"],
    ["###", "#.#", "###", "..#", "###"],
]
TOUCH_WHITE_LEFT = TOUCH_PAD
TOUCH_DIGITS_LEFT = TOUCH_PAD + 4


def touch_icon_rows(name):
    for icon_name, rows in TOUCH_ICONS:
        if icon_name == name:
            return rows
    raise KeyError(name)


def centred(rows):
    # Moves the shape to the middle of its cell. When it cannot sit exactly in
    # the middle it goes up and left, leaving the spare texel to its shadow.
    filled = [(x, y) for y, row in enumerate(rows) for x, c in enumerate(row) if c == "#"]
    left = min(x for x, _ in filled)
    right = max(x for x, _ in filled)
    top = min(y for _, y in filled)
    bottom = max(y for _, y in filled)
    shift_x = (TOUCH_ICON - (right - left + 1)) // 2 - left
    shift_y = (TOUCH_ICON - (bottom - top + 1)) // 2 - top
    out = [["."] * TOUCH_ICON for _ in range(TOUCH_ICON)]
    for x, y in filled:
        out[y + shift_y][x + shift_x] = "#"
    return ["".join(row) for row in out]


def touch_icons():
    # The d-pad's other arrows are turns of the up arrows.
    icons = list(TOUCH_ICONS)
    up = touch_icon_rows("arrow_up")
    right = rotate(up)
    down = rotate(right)
    left = rotate(down)
    up_left = touch_icon_rows("arrow_up_left")
    up_right = rotate(up_left)
    icons += [("arrow_down", down), ("arrow_left", left), ("arrow_right", right), ("arrow_up_right", up_right)]
    icons += TOUCH_EXTRA_ICONS
    return [(name, centred(rows)) for name, rows in icons]


def touch_frame(face, top_left, bottom_right):
    rows = []
    last = TOUCH_BUTTON - 1
    for y in range(TOUCH_BUTTON):
        row = ""
        for x in range(TOUCH_BUTTON):
            corner = (x in (0, last)) and (y in (0, last))
            if corner:
                row += "."
            elif x in (0, last) or y in (0, last):
                row += "K"
            elif x == 1 or y == 1:
                row += top_left
            elif x == last - 1 or y == last - 1:
                row += bottom_right
            elif x == 2 or y == 2 or x == last - 2 or y == last - 2:
                row += "F"
            else:
                row += face
        rows.append(row)
    return rows


def edge_depth(inside, x, y, limit):
    for depth in range(limit):
        reach = depth + 1
        for dy in range(-reach, reach + 1):
            for dx in range(-reach, reach + 1):
                if not inside(x + dx, y + dy):
                    return depth
    return limit


def touch_pad():
    rows = []
    inside = rounded_rect(TOUCH_PAD, TOUCH_PAD, 6.0, 6.0)
    for y in range(TOUCH_PAD):
        row = ""
        for x in range(TOUCH_PAD):
            if not inside(x, y):
                row += "."
                continue
            depth = edge_depth(inside, x, y, 3)
            if depth == 0:
                row += "K"
            elif depth == 1:
                row += "W" if (x < TOUCH_PAD - y) else "S"
            elif depth == 2:
                row += "F"
            else:
                row += "G"
        rows.append(row)
    return rows


def write_touch(path):
    icons = touch_icons()
    width = max(TOUCH_BUTTON * 3, TOUCH_ICON * len(icons))
    height = TOUCH_BUTTON + TOUCH_ICON + TOUCH_PAD
    pixels = [TOUCH_COLOURS["."]] * (width * height)

    def put(left, top, rows, colours):
        for y, row in enumerate(rows):
            for x, c in enumerate(row):
                if c != ".":
                    pixels[(top + y) * width + left + x] = colours[c]

    frames = [touch_frame("I", "W", "S"), touch_frame("P", "S", "W"), touch_frame("L", "W", "W")]
    for index, frame in enumerate(frames):
        put(index * TOUCH_BUTTON, 0, frame, TOUCH_COLOURS)

    white = (255, 255, 255, 255)
    shadow = (24, 24, 24, 200)
    for index, (_, rows) in enumerate(icons):
        left = index * TOUCH_ICON
        top = TOUCH_BUTTON
        for y in range(TOUCH_ICON):
            for x in range(TOUCH_ICON):
                if rows[y][x] == "#":
                    continue
                if x > 0 and y > 0 and rows[y - 1][x - 1] == "#":
                    pixels[(top + y) * width + left + x] = shadow
        for y in range(TOUCH_ICON):
            for x in range(TOUCH_ICON):
                if rows[y][x] == "#":
                    pixels[(top + y) * width + left + x] = white

    put(0, TOUCH_BUTTON + TOUCH_ICON, touch_pad(), TOUCH_COLOURS)
    top = TOUCH_BUTTON + TOUCH_ICON
    for y in range(4):
        for x in range(4):
            pixels[(top + y) * width + TOUCH_WHITE_LEFT + x] = white
    for index, rows in enumerate(TOUCH_DIGITS):
        for y, row in enumerate(rows):
            for x, c in enumerate(row):
                if c == "#":
                    pixels[(top + y) * width + TOUCH_DIGITS_LEFT + index * 4 + x] = white
    write_png(path, width, height, pixels)


def main():
    os.makedirs(OUT, exist_ok=True)
    write_png(os.path.join(OUT, "keycap.png"), len(KEYCAP[0]), len(KEYCAP), paint(KEYCAP))

    icons = [mouse_icon(LEFT_BUTTON, "A"), mouse_icon(RIGHT_BUTTON, "A"), mouse_icon(WHEEL, "A"),
             wheel_icon(-1), wheel_icon(1), MOUSE]
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
    for name, (width, height, inside, ring) in PAD_SHAPES.items():
        rows = shade(width, height, inside, ring)
        write_png(os.path.join(OUT, "pad_%s.png" % name), width, height, [MARK[c] for row in rows for c in row])

    pixels = []
    for y in range(9):
        for symbol in SYMBOLS:
            pixels.extend(white if c == "#" else clear for c in symbol[y])
    write_png(os.path.join(OUT, "symbols.png"), 9 * len(SYMBOLS), 9, pixels)
    write_touch(os.path.join(OUT, "touch.png"))
    print("wrote", OUT)


if __name__ == "__main__":
    main()
