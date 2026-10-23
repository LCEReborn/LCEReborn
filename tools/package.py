#!/usr/bin/env python3
"""Package the built RPX as a title folder that Cemu runs.

    python tools/package.py --update <update dump>

Run tools/prepare.py and tools/build.py first. This writes build/title/:

  code/      Minecraft.Client.rpx from tools/build.py, with app.xml and
             cos.xml from the update dump (cos.xml also allows SD card access)
  meta/      the update dump's meta/
  content/   a link to assets/, or a copy of it with --copy

Start it with

    Cemu -g build/title/code/Minecraft.Client.rpx

Cemu mounts the folder as the update of the base game, so the base game must
be in Cemu's game list or installed in Cemu.

Options:
  --copy     copy assets/ into content/ instead of linking it, for a folder
             that can be moved elsewhere
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
sys.path.insert(0, os.path.join(TOOLS, "gamedata"))
import prepare  # noqa: E402
import shaders  # noqa: E402

BUILD_DIR = os.path.join(ROOT, "build")
RPX = os.path.join(BUILD_DIR, "Minecraft.Client.rpx")
TITLE_DIR = os.path.join(BUILD_DIR, "title")
CODE_FILES = ("app.xml", "cos.xml")
FS_PERMISSION_GROUP = 11
SD_CARD_PERMISSIONS = 0x000B0000


class PackageError(Exception):
    """An input is missing or the title folder cannot be written."""


def check_inputs(update):
    if not os.path.isfile(RPX):
        raise PackageError("%s is missing. Build it first: python tools/build.py" % os.path.relpath(RPX, ROOT))
    manifest = os.path.join(prepare.ASSETS_DIR, prepare.MANIFEST)
    if not os.path.isfile(manifest):
        raise PackageError("assets/ has not been prepared. Run tools/prepare.py with your base game "
                           "and update dumps first.")
    try:
        prepare.check_dump(update, "update", shaders.load_recipe())
    except prepare.PrepareError as error:
        raise PackageError(str(error))
    for name in CODE_FILES:
        if not os.path.isfile(os.path.join(update, "code", name)):
            raise PackageError("update dump %s has no code/%s" % (update, name))
    with open(manifest, encoding="utf-8") as handle:
        return json.load(handle)


def remove_content(path):
    """Remove content/ without ever deleting through a link into assets/."""
    if os.path.islink(path):
        os.unlink(path)
    elif os.path.isdir(path):
        try:
            # A Windows junction is removed by rmdir and its target is left alone;
            # an empty folder goes the same way.
            os.rmdir(path)
        except OSError:
            if getattr(os.path, "isjunction", lambda _: False)(path):
                raise PackageError("cannot remove the content link %s" % path)
            shutil.rmtree(path)
    elif os.path.exists(path):
        os.unlink(path)


def link_content(target, link):
    target, link = os.path.abspath(target), os.path.abspath(link)
    if os.name == "nt":
        # A junction needs no administrator rights or developer mode, unlike a symlink.
        result = subprocess.run(["cmd", "/c", "mklink", "/J", link, target],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if result.returncode != 0:
            raise PackageError("could not link %s to %s: %s"
                               % (link, target, result.stderr.decode("mbcs", "replace").strip()))
    else:
        os.symlink(target, link, target_is_directory=True)


def copy_tree(source, target):
    shutil.copytree(source, target, ignore=shutil.ignore_patterns(prepare.MANIFEST))


def grant_sd_card(path):
    """Let the title read, write and mount the SD card, where the packs folder lives."""
    with open(path, encoding="utf-8", newline="") as handle:
        text = handle.read()
    pattern = re.compile(r'(<group type="unsignedInt" length="4">\s*%d</group>\s*<mask type="hexBinary" length="8">)'
                         r'([0-9A-Fa-f]{16})(</mask>)' % FS_PERMISSION_GROUP)
    match = pattern.search(text)
    if match is None:
        raise PackageError("%s has no FS permission group" % path)
    mask = "%016X" % (int(match.group(2), 16) | SD_CARD_PERMISSIONS)
    text = text[:match.start(2)] + mask + text[match.end(2):]
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write(text)


def package(update, copy):
    content = os.path.join(TITLE_DIR, "content")
    remove_content(content)
    for folder in ("code", "meta"):
        path = os.path.join(TITLE_DIR, folder)
        if os.path.exists(path):
            shutil.rmtree(path)
    code = os.path.join(TITLE_DIR, "code")
    os.makedirs(code)
    shutil.copyfile(RPX, os.path.join(code, os.path.basename(RPX)))
    for name in CODE_FILES:
        shutil.copyfile(os.path.join(update, "code", name), os.path.join(code, name))
    grant_sd_card(os.path.join(code, "cos.xml"))
    copy_tree(os.path.join(update, "meta"), os.path.join(TITLE_DIR, "meta"))
    if copy:
        print("copying assets/ into %s" % os.path.relpath(content, ROOT), flush=True)
        copy_tree(prepare.ASSETS_DIR, content)
    else:
        link_content(prepare.ASSETS_DIR, content)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--update", required=True, help="extracted update v688 dump (folder with code/, content/, meta/)")
    parser.add_argument("--copy", action="store_true", help="copy assets/ into content/ instead of linking it")
    args = parser.parse_args(argv)
    try:
        check_inputs(args.update)
        package(args.update, args.copy)
    except (PackageError, OSError) as error:
        print("error: %s" % error, file=sys.stderr)
        return 1
    rpx = os.path.join(TITLE_DIR, "code", os.path.basename(RPX))
    print("packaged %s" % os.path.relpath(TITLE_DIR, ROOT))
    print("run it with: Cemu -g \"%s\"" % rpx)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
