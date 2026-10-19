#!/usr/bin/env python3
"""Check the launcher's commit blocks against the game's block atlas.

    python tools/check_launcher_blocks.py

launcher/BlockIcons.cpp lists the solid blocks a commit's icon is chosen from,
each with the atlas tiles of its top, front and side. This checks that every
tile's row and column are the ones source/game/client/render/TextureAtlas.cpp
gives that texture, that block names are unique, and that the first blocks,
the ones released commits were given, are unchanged. New blocks may only be
added at the end of the list.
"""
import hashlib
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ATLAS = os.path.join(ROOT, "source", "game", "client", "render", "TextureAtlas.cpp")
BLOCKS = os.path.join(ROOT, "launcher", "BlockIcons.cpp")
PINNED_COUNT = 82
PINNED_DIGEST = "56a759401083657127fbff9e5ce82f0843ec751277dc44766eb3635b3143929e"

ENTRY = re.compile(r'\{"(\w+)", \{"(\w+)", (\d+), (\d+)\}, \{"(\w+)", (\d+), (\d+)\}, \{"(\w+)", (\d+), (\d+)\}\}')


def digest(entries):
    lines = ["%s:%s:%s:%s" % (entry[0], entry[1], entry[4], entry[7]) for entry in entries]
    return hashlib.sha256("\n".join(lines).encode("utf-8")).hexdigest()


def main():
    with open(ATLAS, encoding="utf-8") as handle:
        atlas = {name: (int(row), int(column))
                 for row, column, name in re.findall(r'addIcon\(true, (\d+), (\d+), L"(\w+)"\)', handle.read())}
    with open(BLOCKS, encoding="utf-8") as handle:
        entries = ENTRY.findall(handle.read())

    errors = []
    if not atlas:
        errors.append("no block textures found in %s" % os.path.relpath(ATLAS, ROOT))
    names = [entry[0] for entry in entries]
    for name in sorted(set(name for name in names if names.count(name) > 1)):
        errors.append("the block %s is listed more than once" % name)
    for entry in entries:
        for texture, row, column in (entry[1:4], entry[4:7], entry[7:10]):
            if texture not in atlas:
                errors.append("%s: the game has no texture called %s" % (entry[0], texture))
            elif atlas[texture] != (int(row), int(column)):
                errors.append("%s: %s is at row %d, column %d in the game, not %s, %s"
                              % (entry[0], texture, atlas[texture][0], atlas[texture][1], row, column))
    if len(entries) < PINNED_COUNT or digest(entries[:PINNED_COUNT]) != PINNED_DIGEST:
        errors.append("the first %d blocks changed; released commits would get different blocks. "
                      "Only add blocks at the end of the list." % PINNED_COUNT)

    for error in errors:
        print("error: %s" % error, file=sys.stderr)
    if errors:
        return 1
    print("%d blocks match the game's atlas" % len(entries))
    return 0


if __name__ == "__main__":
    sys.exit(main())
