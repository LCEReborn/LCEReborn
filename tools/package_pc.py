#!/usr/bin/env python3
"""Package the PC build as a zip to give to players.

    python tools/package_pc.py

Build the game and the launcher first (docs/pc-platform.md). This writes
build/LCEReborn-pc-<commit>.zip holding the two programs, the Qt libraries
the launcher uses, resources/, a README, the project's license and, in
third_party/, the notices and licenses of every library in the programs.

Only those files are taken from the build folder, so the game files, saves,
logs and settings that playing from it leaves there stay out.

The zip names the commit it was built from as its source, so the tree must
have no uncommitted changes and the commit should be pushed before the zip is
given out.

Options:
  --build DIR     the PC build folder (default build/pc)
  --allow-dirty   package a tree with uncommitted changes, for testing; the
                  zip is named ...-dirty and must not be given out, since its
                  source is not published
"""
import argparse
import datetime
import glob
import json
import os
import re
import subprocess
import sys
import textwrap
import zipfile

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
DEFAULT_BUILD = os.path.join(ROOT, "build", "pc")
OUTPUT_DIR = os.path.join(ROOT, "build")
THIRD_PARTY = os.path.join(ROOT, "build", "third_party")
LICENSE_TEXTS = os.path.join(TOOLS, "package", "licenses")
TOP = "LCEReborn"

PROGRAMS = ("LCEReborn.exe", "LCERebornLauncher.exe")
QT_PLUGIN_DIRS = ("generic", "imageformats", "networkinformation", "platforms", "styles", "tls")
ALLOWED_EXTENSIONS = (".exe", ".dll", ".png", ".ico", ".txt")
REDIST_URL = "https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist"


class PackageError(Exception):
    """An input is missing or the package cannot be made."""


def git(*args):
    result = subprocess.run(("git", "-C", ROOT) + args, capture_output=True, text=True)
    if result.returncode != 0:
        raise PackageError("git %s failed: %s" % (" ".join(args), result.stderr.strip()))
    return result.stdout.strip()


def source_info(allow_dirty):
    commit = git("rev-parse", "HEAD")
    dirty = git("status", "--porcelain", "--untracked-files=no") != ""
    if dirty and not allow_dirty:
        raise PackageError("The tree has uncommitted changes, so no published source matches the build. "
                           "Commit them first, or pass --allow-dirty for a test package.")
    if not git("branch", "-r", "--contains", commit):
        print("warning: %s is not on any remote branch; push it before giving out the zip" % commit[:12])
    url = git("remote", "get-url", "origin")
    match = re.match(r"(?:https://|git@)github\.com[/:](.+?)(?:\.git)?$", url)
    if not match:
        raise PackageError("origin (%s) is not a GitHub repository, so the source link cannot be written" % url)
    timestamp = int(git("show", "-s", "--format=%ct", commit))
    return {
        "commit": commit,
        "dirty": dirty,
        "repository": "https://github.com/" + match.group(1),
        "date": datetime.datetime.fromtimestamp(timestamp, datetime.timezone.utc),
    }


def read(path):
    if not os.path.isfile(path):
        raise PackageError("%s is missing" % os.path.relpath(path, ROOT))
    with open(path, encoding="utf-8", errors="replace") as handle:
        return handle.read()


def block(text, start, end, path):
    match = re.search(re.escape(start) + r".*?" + re.escape(end), text, re.S)
    if not match:
        raise PackageError("The license notice in %s was not found" % os.path.relpath(path, ROOT))
    return match.group(0)


def uncomment(text):
    lines = [re.sub(r"^\s*(//|\*|/\*)", "", line).rstrip() for line in text.splitlines()]
    indents = [len(line) - len(line.lstrip()) for line in lines[1:] if line.strip()]
    indent = min(indents) if indents else 0
    return "\n".join([lines[0].strip()] + [line[indent:] for line in lines[1:]]).strip()


def cache_value(build, name):
    text = read(os.path.join(build, "CMakeCache.txt"))
    match = re.search(r"^%s:[A-Z]+=(.*)$" % re.escape(name), text, re.M)
    if not match:
        raise PackageError("%s is not set in %s/CMakeCache.txt" % (name, os.path.relpath(build, ROOT)))
    return match.group(1).strip()


def program_files(build):
    files = []
    for name in PROGRAMS:
        path = os.path.join(build, name)
        if not os.path.isfile(path):
            raise PackageError("%s is missing. Build the game and the launcher first." % os.path.relpath(path, ROOT))
        files.append((path, name))
    qt_libraries = sorted(glob.glob(os.path.join(build, "Qt6*.dll")))
    if not qt_libraries:
        raise PackageError("No Qt libraries in %s. Build the launcher, which deploys them." % build)
    for path in qt_libraries:
        files.append((path, os.path.basename(path)))
    for folder in QT_PLUGIN_DIRS:
        for path in sorted(glob.glob(os.path.join(build, folder, "*.dll"))):
            files.append((path, folder + "/" + os.path.basename(path)))
    resources = os.path.join(ROOT, "resources", "pc")
    for folder, _, names in os.walk(resources):
        for name in sorted(names):
            path = os.path.join(folder, name)
            files.append((path, "resources/" + os.path.relpath(path, resources).replace(os.sep, "/")))
    return files


def license_ids(expression):
    tokens = re.findall(r"[A-Za-z0-9.+\-]+", expression)
    return [token for token in tokens if token not in ("AND", "OR", "WITH")]


def qt_notices(build, shipped):
    qt_root = os.path.normpath(os.path.join(cache_value(build, "Qt6_DIR"), "..", "..", ".."))
    sboms = glob.glob(os.path.join(qt_root, "sbom", "qtbase-*.spdx.json"))
    if len(sboms) != 1:
        raise PackageError("Expected one qtbase SBOM in %s/sbom" % qt_root)
    with open(sboms[0], encoding="utf-8") as handle:
        sbom = json.load(handle)

    packages = dict((package["SPDXID"], package) for package in sbom["packages"])
    files = dict((entry["SPDXID"], entry["fileName"]) for entry in sbom["files"])
    owner = {}
    depends = {}
    for relation in sbom["relationships"]:
        kind = relation["relationshipType"]
        if kind == "CONTAINS" and relation["relatedSpdxElement"] in files:
            owner[files[relation["relatedSpdxElement"]]] = relation["spdxElementId"]
        elif kind == "DEPENDS_ON":
            depends.setdefault(relation["spdxElementId"], []).append(relation["relatedSpdxElement"])

    modules = []
    for arcname in shipped:
        if not arcname.lower().endswith(".dll"):
            continue
        sbom_name = "./bin/" + arcname if "/" not in arcname else "./plugins/" + arcname
        if sbom_name not in owner:
            raise PackageError("%s is not described by Qt's SBOM, so its notices are unknown" % arcname)
        if owner[sbom_name] not in modules:
            modules.append(owner[sbom_name])

    reached = []
    pending = list(modules)
    while pending:
        spdx_id = pending.pop(0)
        if spdx_id in reached or spdx_id not in packages:
            continue
        reached.append(spdx_id)
        pending.extend(dependency for dependency in depends.get(spdx_id, []) if "3rdparty" in dependency)

    attributions = []
    seen = set()
    for spdx_id in reached:
        package = packages[spdx_id]
        if "3rdparty" not in spdx_id or package.get("licenseConcluded", "NOASSERTION") == "NOASSERTION":
            continue
        key = (package["licenseConcluded"], package.get("copyrightText", ""))
        if key in seen:
            continue
        seen.add(key)
        attributions.append(package)

    core = [package for package in sbom["packages"] if package["name"] == "Core"][0]
    commit = core["downloadLocation"].rsplit("@", 1)[-1]
    version = core["versionInfo"]
    extracted = dict((info["licenseId"], info["extractedText"]) for info in sbom.get("hasExtractedLicensingInfos", []))
    return {
        "version": version,
        "commit": commit,
        "modules": [packages[spdx_id]["name"] for spdx_id in modules],
        "attributions": attributions,
        "extracted": extracted,
    }


def match(pattern, text, path):
    found = re.search(pattern, text)
    if not found:
        raise PackageError("Could not read %r from %s" % (pattern, os.path.relpath(path, ROOT)))
    return found.group(1)


def game_components(build):
    deps = json.loads(read(os.path.join(TOOLS, "build", "deps.json")))
    sdl_source = os.path.join(build, "_deps", "sdl3-src")
    version_path = os.path.join(sdl_source, "include", "SDL3", "SDL_version.h")
    version_header = read(version_path)
    sdl_version = ".".join(match(r"#define SDL_%s_VERSION\s+(\d+)" % part, version_header, version_path)
                           for part in ("MAJOR", "MINOR", "MICRO"))

    zlib_header = os.path.join(THIRD_PARTY, "zlib", "zlib.h")
    stb_image = os.path.join(THIRD_PARTY, "stb", "stb_image.h")
    stb_truetype = os.path.join(ROOT, "source", "vendor", "stb_truetype.hpp")
    rapidxml = os.path.join(TOOLS, "build", "third_party", "rapidxml", "rapidxml.hpp")
    bink = os.path.join(ROOT, "source", "platform", "pc", "audio", "BinkAudioDecoder.cpp")
    boost_license = os.path.join(LICENSE_TEXTS, "BSL-1.0.txt")
    rapidxml_header = read(rapidxml)
    truetype_header = read(stb_truetype)

    return [
        {
            "name": "SDL",
            "version": sdl_version,
            "source": "https://github.com/libsdl-org/SDL",
            "license": ["Zlib"],
            "notice": read(os.path.join(sdl_source, "LICENSE.txt")).strip(),
        },
        {
            "name": "zlib",
            "version": deps["zlib"]["version"],
            "source": "https://zlib.net",
            "license": ["Zlib"],
            "notice": uncomment(block(read(zlib_header), "Copyright (C)", "madler@alumni.caltech.edu", zlib_header)),
        },
        {
            "name": "libpng",
            "version": deps["libpng"]["version"] + ", with this project's changes to png.h and pngwrite.c",
            "source": "https://github.com/pnggroup/libpng",
            "license": ["Libpng"],
            "notice": read(os.path.join(THIRD_PARTY, "libpng", "LICENSE")).strip(),
        },
        {
            "name": "Boost",
            "version": deps["boost"]["version"] + ", headers",
            "source": "https://www.boost.org",
            "license": ["BSL-1.0"],
            "notice": read(boost_license).strip(),
        },
        {
            "name": "RapidXml",
            "version": match(r"// Version ([\d.]+)", rapidxml_header, rapidxml),
            "source": "https://rapidxml.sourceforge.net",
            "license": ["BSL-1.0"],
            "notice": match(r"// (Copyright .*)", rapidxml_header, rapidxml)
                      + "\nUsed under the Boost Software License 1.0.",
        },
        {
            "name": "stb_image",
            "version": deps["stb_image"]["version"],
            "source": "https://github.com/nothings/stb",
            "license": ["public domain"],
            "notice": uncomment(block(read(stb_image), "This software is dual-licensed", "see fit.", stb_image)),
        },
        {
            "name": "stb_truetype",
            "version": match(r"stb_truetype\.h - v([\d.]+)", truetype_header, stb_truetype),
            "source": "https://github.com/nothings/stb",
            "license": ["public domain"],
            "notice": uncomment(block(truetype_header, "This software is dual-licensed", "see fit.", stb_truetype)),
        },
        {
            "name": "Bink Audio decoder, ported from FFmpeg",
            "version": "libavformat/binka.c and libavcodec/binkaudio.c",
            "source": "https://ffmpeg.org",
            "license": ["GPL-3.0-only"],
            "notice": uncomment(block(read(bink), "The container parsing", "License, as that license allows.", bink)),
        },
    ]


def rule(title):
    return "%s\n%s\n" % (title, "=" * len(title))


def notices_text(source, game, qt):
    out = [rule("Third-party notices")]
    out.append("LCEReborn is licensed under the GNU General Public License, version 3\n"
               "or later (LICENSE.txt). It contains the libraries below. The texts of\n"
               "the licenses named here are in licenses/.\n")

    out.append(rule("In LCEReborn.exe"))
    for component in game:
        out.append("%s %s\n%s\nLicense: %s\n\n%s\n" % (component["name"], component["version"], component["source"],
                                                     ", ".join(component["license"]), component["notice"]))
        out.append("-" * 72 + "\n")

    out.append(rule("Qt, in LCERebornLauncher.exe"))
    archive = "https://download.qt.io/archive/qt/%s/%s/submodules/qtbase-everywhere-src-%s.tar.xz" % (
        ".".join(qt["version"].split(".")[:2]), qt["version"], qt["version"])
    out.append("The launcher uses Qt %s (qtbase): the Qt6*.dll libraries and the plugin\n"
               "folders beside it. Copyright (C) The Qt Company Ltd. and other\n"
               "contributors. Qt is used under the GNU Lesser General Public License,\n"
               "version 3 (licenses/LGPL-3.0-only.txt, which adds to\n"
               "licenses/GPL-3.0-only.txt).\n\n"
               "The Qt libraries are unmodified and loaded at run time, so you can\n"
               "replace them with your own build of Qt %s. Their source code is at\n"
               "%s\n"
               "and in git at https://code.qt.io/qt/qtbase.git, commit\n"
               "%s.\n\n"
               "%s\n\n"
               "The Qt libraries contain the following third-party code.\n\n"
               % (qt["version"], qt["version"], archive, qt["commit"],
                  textwrap.fill("Modules shipped: %s." % ", ".join(qt["modules"]), 72)))
    for package in qt["attributions"]:
        details = re.search(r"Name: (.*)", package.get("comment", ""))
        lines = [details.group(1).strip() if details else package["name"].split("_Attribution_")[-1]]
        if package.get("versionInfo", "unknown") != "unknown":
            lines[0] += " " + package["versionInfo"]
        if package.get("downloadLocation", "NOASSERTION") != "NOASSERTION":
            lines.append(package["downloadLocation"])
        lines.append("License: %s" % package["licenseConcluded"])
        if package.get("copyrightText", "NOASSERTION") != "NOASSERTION":
            lines.append(package["copyrightText"].strip())
        out.append("\n".join(lines) + "\n")
    out.append(rule("Source"))
    out.append("The source of this build is at\n%s/tree/%s\n" % (source["repository"], source["commit"]))
    return "\n".join(out)


def license_files(game, qt):
    needed = set(["GPL-3.0-only", "LGPL-3.0-only"])
    for component in game:
        needed.update(identifier for identifier in component["license"] if identifier != "public domain")
    for package in qt["attributions"]:
        needed.update(license_ids(package["licenseConcluded"]))
    needed.discard("LicenseRef-Qt-Commercial")

    files = []
    for identifier in sorted(needed):
        if identifier in qt["extracted"]:
            files.append(("third_party/licenses/%s.txt" % identifier, qt["extracted"][identifier].strip() + "\n"))
            continue
        path = os.path.join(LICENSE_TEXTS, identifier + ".txt")
        if not os.path.isfile(path):
            raise PackageError("No text for the license %s. Add it to %s from its upstream source."
                               % (identifier, os.path.relpath(LICENSE_TEXTS, ROOT)))
        files.append(("third_party/licenses/%s.txt" % identifier, read(path)))
    return files


def readme_text(source):
    readme = read(os.path.join(ROOT, "README.md"))
    disclaimers = block(readme, "## Disclaimers", "\n## ", os.path.join(ROOT, "README.md"))
    disclaimers = disclaimers[len("## Disclaimers"):-len("\n## ")].strip()
    build = source["commit"][:12] + (" with uncommitted changes" if source["dirty"] else "")
    return ("LCEReborn for PC\n"
            "================\n\n"
            "A PC build of LCEReborn, the reconstructed source of Minecraft: Wii U\n"
            "Edition. Build %s.\n\n"
            "This download contains no game content. Start LCERebornLauncher.exe:\n"
            "the first time, it asks for your own dumps of the base game and update\n"
            "688 and imports them into the folder beside it. Everything the launcher\n"
            "and the game keep, including saves, stays in this folder.\n\n"
            "It needs the Microsoft Visual C++ Redistributable for Visual Studio\n"
            "2015-2022 (x64), which most Windows PCs already have:\n"
            "%s\n\n"
            "Source and license\n"
            "------------------\n\n"
            "LCEReborn is free software under the GNU General Public License,\n"
            "version 3 or later (LICENSE.txt). The source of this build is at\n"
            "%s/tree/%s\n\n"
            "third_party/NOTICES.txt lists the libraries in it and their licenses.\n\n"
            "Disclaimers\n"
            "-----------\n\n"
            "%s\n" % (build, REDIST_URL, source["repository"], source["commit"], disclaimers))


def check_names(entries):
    for arcname, _ in entries:
        if os.path.splitext(arcname)[1].lower() not in ALLOWED_EXTENSIONS:
            raise PackageError("Refusing to package %s: not a program, library, image or text file" % arcname)


def write_zip(path, entries, date):
    stamp = date.timetuple()[:6]
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for arcname, content in entries:
            info = zipfile.ZipInfo(TOP + "/" + arcname, stamp)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            if isinstance(content, str):
                content = content.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8")
            archive.writestr(info, content)


def package(build, allow_dirty):
    """Writes the zip and returns its path and source_info()."""
    source = source_info(allow_dirty)
    files = program_files(build)
    qt = qt_notices(build, [arcname for _, arcname in files])
    game = game_components(build)

    entries = []
    for path, arcname in files:
        with open(path, "rb") as handle:
            entries.append((arcname, handle.read()))
    entries.append(("README.txt", readme_text(source)))
    entries.append(("LICENSE.txt", read(os.path.join(ROOT, "LICENSE"))))
    entries.append(("third_party/NOTICES.txt", notices_text(source, game, qt)))
    entries.extend(license_files(game, qt))
    check_names(entries)

    name = "LCEReborn-pc-%s%s.zip" % (source["commit"][:12], "-dirty" if source["dirty"] else "")
    output = os.path.join(OUTPUT_DIR, name)
    write_zip(output, entries, source["date"])
    print("Wrote %s (%d files)" % (os.path.relpath(output, ROOT), len(entries)))
    return output, source


def main():
    parser = argparse.ArgumentParser(description="Package the PC build as a zip to give to players.")
    parser.add_argument("--build", default=DEFAULT_BUILD, help="the PC build folder (default build/pc)")
    parser.add_argument("--allow-dirty", action="store_true",
                        help="package a tree with uncommitted changes, for testing only")
    args = parser.parse_args()

    try:
        package(os.path.abspath(args.build), args.allow_dirty)
    except PackageError as error:
        print("error: %s" % error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
