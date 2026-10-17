# PC launcher

`launcher/` builds `LCERebornLauncher`, a Qt 6 program that sets up the game
files, keeps player profiles, starts the PC game and shows its log. Players
of the PC port use it instead of `tools/prepare.py`: it runs no Python.

## Building it

The launcher needs Qt 6. `tools/build/qt.py` installs the version pinned in
`tools/build/qt.json` (Qt 6.12.0, the `qtbase` archive) into `build/qt/`:

```
python tools/build/qt.py
```

It installs Qt with aqtinstall, pinned to a commit because the 3.3.0 release
cannot read the Qt 6.11 and later repository layout. aqtinstall goes into its
own virtual environment in `build/qt/venv`, so nothing is installed into the
system's Python. Running the script again does nothing once that Qt is in
place. The prebuilt Qt for Windows is the MSVC 2022 build, which clang-cl
links against.

The PC CMake build then builds the launcher next to the game
(`build/pc/LCERebornLauncher.exe`) and, on Windows, copies the Qt libraries
beside it with `windeployqt`. If Qt is missing, CMake stops and names the
script; `-DLCE_BUILD_LAUNCHER=OFF` builds the game alone.

Each build records the commit it was built from and the build date
(`launcher/BuildInfo.cmake`). The launcher shows them in its title bar and
under the Play button, with "(modified)" when the working tree had changes.

## Its folder

The launcher is portable: everything it keeps is in its own folder.

| Path | Holds |
| --- | --- |
| `assets/` | the game content the setup imports |
| `resources/` | the PC interface art, which the build copies there from `resources/pc` |
| `launcher_profiles.json` | the profiles and which one is selected |
| `launcher.ini` | the window's size and tab, the log filters and the dump folders last chosen |
| `launcher_news.json` | the last news fetched, shown when offline |

## Setup

When `assets/` holds no game content, the launcher shows only the setup page.
It asks for the two dumps `prepare.py` takes: the base game (title
`00050000101D9D00`) and update 688. Each folder is checked as it is chosen. The update must report version 688, the base game must not be an
update, and the base game must hold the `Common`, `music` and `WiiU` content
folders.

Importing copies the base game's content and then the update's files over it,
as `prepare.py` does, into `assets.partial/`. Only when every file is copied is
it renamed to `assets/`, with `.prepared.json` recording the inputs. An import
that is cancelled or fails leaves no half-written `assets/`. Free space is
checked before anything is copied. The result is byte for byte what
`prepare.py` writes to `assets/`. The GX2 shaders `prepare.py` also extracts
are only for the Wii U build and are not part of the import.

"Re-import game files" in the top bar runs the setup again.

## Profiles

A profile has a name, a player name, a game folder, a language and what the
launcher does once the game starts (stay open, hide until the game closes, or
close). Each new profile gets a random player UUID that stays the same when
the player name changes, so worlds keep recognising the player. A profile with
no game folder uses the launcher's own folder.

The profile's skin is read from the game's own files once they are imported:
the skin the player chose in the game's options for that UUID, a default skin
from `assets/`, or a DLC skin from the packs. Before the import, profiles show
no face.

## Starting the game

Play starts `LCEReborn` with the profile's settings:

```
LCEReborn --username <player name> --uuid <profile uuid>
          --content-dir <launcher>/assets --resource-dir <launcher>/resources
          --save-dir <game folder>/saves --log-dir <game folder>/logs
          [--language <code>]
```

The Game Log tab shows the game's output as it runs, parsed from the line
format described in `pc-platform.md`. It has a toggle for each level, a
search, and a link to the profile's logs folder.

## News

The News tab shows the release notes of the project's ten latest GitHub
releases. The last response is saved, so the tab still shows it offline, with
the date it was saved.
