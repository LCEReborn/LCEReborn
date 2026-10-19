# PC launcher

`launcher/` builds `LCERebornLauncher`, a Qt 6 program that imports the game
files, keeps profiles, starts the PC game and shows its log. It replaces
`tools/prepare.py` for PC players and runs no Python.

## Building it

```
python tools/build/qt.py
```

installs the Qt pinned in `tools/build/qt.json` into `build/qt/`. The PC CMake
build then builds the launcher next to the game; `-DLCE_BUILD_LAUNCHER=OFF`
skips it.

## Its folder

The launcher is portable and keeps everything beside itself: `assets/` (the
imported game), `resources/`, `license.cfg`, `launcher_profiles.json` and
`launcher.ini`.

## Setup

When there is no imported game, the launcher opens on setup:

1. **Base game**: your dump of the game, checked as you pick it.
2. **Update 688**: your dump of the update.
3. **Licenses**: "Edit license.cfg" opens the license file in a text editor,
   creating it first if needed.
4. **Import**: copies the game into `assets/`.

"Re-import game files" opens setup again.

## Licenses

The game starts as the trial with no DLC until you edit `license.cfg`, beside
the launcher; only unlock what you own. Setup's Licenses step opens it, and
"Re-import game files" brings setup back. Set `full_game = true` for the full
game and `dlc` to `all` or the names of your packs (see Licenses in
`pc-platform.md`).

## Playing

Each profile has its own player name, game folder and language. The Game Log
tab shows the game's output, and News shows the project's releases.
