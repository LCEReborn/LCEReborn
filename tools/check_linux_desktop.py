#!/usr/bin/env python3
"""Check the Linux package's desktop installer using desktop-file-validate and gio.

    python tools/check_linux_desktop.py

Builds a small tar.gz with a stand-in launcher, installs its desktop entry in
a temporary data directory, and launches it through GLib. No game files or
changes to the user's applications menu are needed.
"""
import datetime
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import time

import package_pc


def run(*args, env=None):
    return subprocess.run(args, env=env, check=True, capture_output=True, text=True, timeout=15)


def launch(entry, result, program, env):
    result.unlink(missing_ok=True)
    run("desktop-file-validate", str(entry))
    run("gio", "launch", str(entry), env=env)
    deadline = time.monotonic() + 5
    while not result.exists() and time.monotonic() < deadline:
        time.sleep(0.05)
    assert result.exists(), "The desktop entry did not start the launcher"
    assert result.read_text().strip() == str(program), "The desktop entry started the wrong launcher"


def main():
    for tool in ("desktop-file-validate", "gio"):
        if shutil.which(tool) is None:
            raise SystemExit("%s is needed for this Linux desktop check" % tool)

    with tempfile.TemporaryDirectory(prefix="lcereborn-desktop-") as temp:
        root = Path(temp)
        install = root / 'LCE "quoted" $HOME `tick` 100% = back\\slash café'
        data = root / "user data"
        result = root / "launched.txt"
        env = dict(os.environ, XDG_DATA_HOME=str(data), LCE_DESKTOP_TEST_RESULT=str(result))
        entries = package_pc.desktop_entries() + [
            ("LCERebornLauncher", '#!/bin/sh\nreadlink -f "$0" > "$LCE_DESKTOP_TEST_RESULT"\n')]
        package_pc.check_names(entries)
        archive = root / "desktop.tar.gz"
        package_pc.write_tar(str(archive), entries, datetime.datetime.now(datetime.timezone.utc))
        with tarfile.open(archive) as packed:
            for name, _ in entries:
                member = packed.getmember(package_pc.TOP + "/" + name)
                target = install / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(packed.extractfile(member).read())
                target.chmod(member.mode)
        script = install / package_pc.DESKTOP_SCRIPT
        assert os.access(script, os.X_OK), "The tar.gz installer is not executable"
        entry = data / "applications" / (package_pc.DESKTOP_ID + ".desktop")
        icon = data / "icons/hicolor/256x256/apps" / (package_pc.DESKTOP_ID + ".png")

        run(str(script), env=env)
        launch(entry, result, install / "LCERebornLauncher", env)
        assert icon.read_bytes() == (install / "share/icons/hicolor/256x256/apps" / icon.name).read_bytes()

        moved = root / 'Moved 50% "elsewhere"'
        install.rename(moved)
        script = moved / package_pc.DESKTOP_SCRIPT
        run(str(script), env=env)
        launch(entry, result, moved / "LCERebornLauncher", env)
        run(str(script), "--remove", env=env)
        assert not entry.exists() and not icon.exists(), "Removal left desktop files behind"
        run(str(script), "--remove", env=env)

        (moved / "LCERebornLauncher").unlink()
        missing = subprocess.run([str(script)], env=env, capture_output=True)
        assert missing.returncode != 0 and not entry.exists(), "A missing launcher was registered"
        unknown = subprocess.run([str(script), "--unknown"], env=env, capture_output=True)
        assert unknown.returncode != 0, "An unknown installer option was accepted"

    print("PASS: packaged desktop entry launch, escaped paths, relocation and removal")


if __name__ == "__main__":
    main()
