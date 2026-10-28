# LCEReborn

Reconstructed C++ source for Minecraft: Wii U Edition, the 4J Studios
Legacy Console Edition codebase, and a native PC version built from it.

The source is a matching decompilation: it is written by reading the
original executable's compiled code and is checked, function by function,
against that executable. Every file is hand-authored text rather than raw
tool output, and the repository contains no binaries, assets, symbol
tables, disassembly or decompiler output. See `docs/provenance.md` for how
the source is made.

All of the code is written by AI agents. Today Claude coordinates Claude
Opus 5.5 worker agents; earlier phases had the dispatchers Sol and Fable
hand class families to Luna and Opus 5 workers. See `docs/overview.md`.

## Playing on PC

1. Download the package for your system from the latest
   [release](https://github.com/LCEReborn/LCEReborn/releases/latest). On
   64-bit Windows, extract the zip anywhere; it needs the Microsoft Visual
   C++ Redistributable most PCs already have. On 64-bit Linux, make the
   AppImage executable and run it, or extract the tar.gz (its README lists
   what Linux needs).
2. Run `LCERebornLauncher` and point its setup at your own dumps of the
   base game and update 688. It imports the game's content beside itself on
   Windows and into `~/.local/share/LCEReborn` on Linux; nothing is
   downloaded.
3. Pick a profile and press Play.

The PC version runs in a window of any shape or borderless fullscreen, with
keyboard and mouse, a touchscreen or controllers, split screen, render distance
up to 32 chunks, and multiplayer with other PCs and with LCEReborn's Wii U
build.
`LCERebornServer` in the same package runs a dedicated server on Windows or
Linux. See `docs/pc-launcher.md` and `docs/pc-platform.md`.

## Building

```
git clone --recurse-submodules https://github.com/LCEReborn/LCEReborn.git
```

The Wii U build makes `Minecraft.Client.rpx` with RedHills, an open-source
clang that keeps the class layout of the compiler the game was built with,
and runs in Docker:

```
python tools/prepare.py --base <base game dump> --update <update v688 dump>
python tools/build.py
python tools/package.py --update <update v688 dump>
Cemu -g build/title/code/Minecraft.Client.rpx
```

See `docs/building.md`. The PC build uses CMake, with clang-cl from a Visual
Studio x64 developer prompt on Windows and clang or GCC on Linux; see Building
on PC in `docs/pc-platform.md`.

The game files come from your own dumps of the base game and update 688.

## Status

The Wii U build runs in a stock Cemu 2.6: it boots to the menus, creates and
loads worlds, and its server has run at the full 20 ticks per second in
testing. The PC version plays on Windows and Linux, and the two builds can play
together. Both are still being refined toward the original function by
function.

- [docs/overview.md](docs/overview.md) - what is here, what is not, and status
- [docs/building.md](docs/building.md) - preparing game files, building the RPX and running it in Cemu
- [docs/pc-platform.md](docs/pc-platform.md) - the PC version: building, files, multiplayer, display, sound and releases
- [docs/pc-launcher.md](docs/pc-launcher.md) - the PC launcher: setup and profiles
- [docs/pc-input.md](docs/pc-input.md) - keyboard, mouse and controllers on PC
- [docs/measuring.md](docs/measuring.md) - comparing your build with your copy of the original, function by function
- [docs/architecture.md](docs/architecture.md) - directory layout and the platform seam
- [docs/conventions.md](docs/conventions.md) - language level, naming and file rules
- [docs/provenance.md](docs/provenance.md) - where the source comes from and contribution rules

## Disclaimers

- This is an unofficial fan project. It is not affiliated with, endorsed by
  or supported by Mojang, Microsoft, 4J Studios or Nintendo. Minecraft is a
  trademark of Mojang Synergies AB; Wii U is a trademark of Nintendo.
- The repository contains no game content. Building and running it needs
  your own copy of the game, dumped from your own console.
- The code is written by AI agents and is still being refined. It does not
  match the original everywhere and may contain bugs the original does not
  have.
- The Wii U build has been tested in Cemu only. Running it on a Wii U
  console has not been tried; if you do, it is at your own risk.
- Back up your saves before using it. It uses the game's own save code, but a
  bug can still corrupt a world.
- Nintendo ended the Wii U's online services in April 2024. LCEReborn does
  not use them: its multiplayer is its own, over your local network or to a
  server's address.
- The software is provided as is, without warranty of any kind; see the
  license.

## License

The reconstructed source and the tooling in this repository are released
under the GNU General Public License, version 3 or (at your option) any later
version. See `LICENSE`.

The license covers what this repository contributes. It grants no rights to
Minecraft: Wii U Edition itself, its code, its assets, or the trademarks of
their owners; because the reconstructed source is derived from the original
executable, see `docs/provenance.md` for what that means.

Files from other projects keep their own licenses:
`tools/build/third_party/rapidxml/` (Boost Software License 1.0), the altered
libpng and curl files in `tools/build/overrides/` (the libpng and curl
licenses), the Bink Audio decoder in
`source/platform/pc/audio/BinkAudioDecoder.cpp`, ported from FFmpeg (GNU
Lesser General Public License 2.1 or later, used here under GPL 3), and the
libraries `tools/build/deps.py` downloads, listed in `docs/building.md`.
The PC build also uses SDL 3 (zlib license) and the launcher Qt 6 (GNU
Lesser General Public License 3); the PC release zip lists everything it
contains, with the licenses, in `third_party/NOTICES.txt`.
`source/nn/ffl` is a submodule holding aboood40091's FFL decompilation, which
its author publishes without a license, together with this project's changes
to it; it is not covered by this repository's license.
