#!/usr/bin/env python3
"""Install the pinned Qt the PC launcher is built with.

    python tools/build/qt.py

Reads tools/build/qt.json and installs that Qt version's base module
(Core, Gui, Widgets, Network and their plugins) for this machine into
build/qt/<version>/<folder>, where CMake looks for it. The installer,
aqtinstall, is pinned to a commit too (its last release predates the
repository layout Qt 6.11 introduced) and lives in its own virtual
environment in build/qt/venv, so nothing is installed system-wide. Running it
again when Qt is already in place does nothing.

Only developers need this; players get Qt's libraries with the launcher.
"""
import json
import os
import platform
import subprocess
import sys
import venv

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
PINS = os.path.join(HERE, "qt.json")
QT_ROOT = os.path.join(ROOT, "build", "qt")
VENV = os.path.join(QT_ROOT, "venv")


class QtError(Exception):
    """Qt could not be installed."""


def host_pins(pins):
    system = platform.system().lower()
    if system not in pins["hosts"]:
        raise QtError("no Qt pin for this system (%s); add one to %s" % (platform.system(), PINS))
    return pins["hosts"][system]


def venv_python():
    if os.name == "nt":
        return os.path.join(VENV, "Scripts", "python.exe")
    return os.path.join(VENV, "bin", "python")


def ensure_installer(requirement):
    python = venv_python()
    if not os.path.isfile(python):
        print("creating %s" % os.path.relpath(VENV, ROOT), flush=True)
        venv.create(VENV, with_pip=True)
    stamp = os.path.join(VENV, "aqtinstall.pin")
    installed = ""
    if os.path.isfile(stamp):
        with open(stamp, encoding="utf-8") as handle:
            installed = handle.read().strip()
    if installed != requirement:
        print("installing %s" % requirement, flush=True)
        result = subprocess.run([python, "-m", "pip", "install", "--disable-pip-version-check", "--force-reinstall",
                                 requirement])
        if result.returncode != 0:
            raise QtError("pip could not install %s" % requirement)
        with open(stamp, "w", encoding="utf-8") as handle:
            handle.write(requirement + "\n")
    return python


def main():
    with open(PINS, encoding="utf-8") as handle:
        pins = json.load(handle)
    host = host_pins(pins)
    prefix = os.path.join(QT_ROOT, pins["version"], host["folder"])
    config = os.path.join(prefix, "lib", "cmake", "Qt6", "Qt6Config.cmake")
    if os.path.isfile(config):
        print("Qt %s is already installed in %s" % (pins["version"], os.path.relpath(prefix, ROOT)))
        return 0

    python = ensure_installer(pins["aqtinstall"])
    command = [python, "-m", "aqt", "install-qt", host["aqt_host"], "desktop", pins["version"], host["arch"],
               "--outputdir", QT_ROOT, "--archives"] + pins["archives"]
    print("+ " + " ".join(command), flush=True)
    if subprocess.run(command).returncode != 0:
        raise QtError("aqtinstall could not install Qt %s for %s" % (pins["version"], host["arch"]))
    if not os.path.isfile(config):
        raise QtError("Qt was installed but %s is missing" % os.path.relpath(config, ROOT))
    print("Qt %s installed in %s" % (pins["version"], os.path.relpath(prefix, ROOT)))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except QtError as error:
        print("error: %s" % error, file=sys.stderr)
        sys.exit(1)
