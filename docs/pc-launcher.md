# PC launcher

`launcher/` builds `LCERebornLauncher`, a Qt 6 program that imports the game
files, keeps profiles, starts the PC game and shows its log. It replaces
`tools/prepare.py` for PC players and runs no Python. Players get it with
the game in the zip of each GitHub release.

## Building it

```
python tools/build/qt.py
```

installs the Qt pinned in `tools/build/qt.json` into `build/qt/`. The PC CMake
build then builds the launcher next to the game; `-DLCE_BUILD_LAUNCHER=OFF`
skips it.

## Its folder

On Windows the launcher is portable and keeps everything beside itself:
`assets/` (the imported game), `license.cfg`, `launcher_profiles.json`,
`launcher.ini` and the default game folder's saves and logs. On Linux these
go in `~/.local/share/LCEReborn` (under `$XDG_DATA_HOME` when it is set),
since an AppImage can't write beside itself. The programs and `resources/`
stay where the launcher is (`launcher/LauncherPaths`).

## Setup

When there is no imported game, the launcher opens on setup:

1. **Base game**: your dump of the game, checked as you pick it.
2. **Update 688**: your dump of the update.
3. **Licenses**: "Edit license.cfg" opens the license file in a text editor,
   creating it first if needed.
4. **Import**: copies the game into `assets/`.

"Re-import game files", in the Profiles tab, opens setup again.

## Licenses

The game starts as the trial with no DLC until you edit `license.cfg`, in the
launcher's folder; only unlock what you own. Setup's Licenses step opens it. Set `full_game = true` for the full
game and `dlc` to `all` or the names of your packs (see Licenses in
`pc-platform.md`).

## Playing

Each profile has its own player name, game folder and language, and whether
split screen is on and other controllers are asked to join when a world loads
(`--split-screen`, `--join-prompt`). The Game Log tab shows the game's output,
and News shows the project's releases. An Update button appears beside Play
when a newer release is out.

## Its look

The launcher draws itself like the game's menus, using the imported game's
font, title panorama and block textures, so it looks plain until the game is
imported.

Each release in News has a block picked from its commit, the same in every
launcher. The blocks are listed in `launcher/BlockIcons.cpp`; add new ones only
at the end of the list, then run

```
python tools/check_launcher_blocks.py
```

to check them against the game's texture atlas.
