# Minecraft: Wii U Edition - source reconstruction

Reconstructed C++ source for Minecraft: Wii U Edition, the 4J Studios
Legacy Console Edition codebase. It is a matching decompilation: the source
is written by reading the original executable's compiled code and is
checked, function by function, against that executable. Every file is
hand-authored text rather than raw tool output, and the repository contains
no binaries, assets, symbol tables, disassembly or decompiler output. See
`docs/provenance.md` for how the source is made.

All of the code is written by AI agents. Today Claude coordinates Claude
Opus 5.5 worker agents; earlier phases had the dispatchers Sol and Fable
hand class families to Luna and Opus 5 workers. See `docs/overview.md`.

```
git clone --recurse-submodules https://github.com/Jacobwasbeast/mcwiiu-reconstructed.git
python tools/prepare.py --base <base game dump> --update <update v688 dump>
python tools/build.py
python tools/package.py --update <update v688 dump>
Cemu -g build/title/code/Minecraft.Client.rpx
```

The game files come from your own dumps of the base game and update 688.
The build compiles the game with RedHills, an open-source clang that keeps
the class layout of the compiler the game was built with, and runs in
Docker. See `docs/building.md`.

## Status

The game builds and runs in a stock Cemu 2.6: it boots to the menus, creates
and loads worlds, and its server has run at the full 20 ticks per second in
testing. It is still being refined toward the original function by function.

- [docs/overview.md](docs/overview.md) - what is here, what is not, and status
- [docs/building.md](docs/building.md) - preparing game files, building the RPX and running it in Cemu
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
- It has been tested in Cemu only. Running it on a Wii U console has not been
  tried; if you do, it is at your own risk.
- Back up your saves before using it. It uses the game's own save code, but a
  bug can still corrupt a world.
- Nintendo ended the Wii U's online services in April 2024, and the online
  features of this build have not been tested.
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
`source/nn/ffl` is a submodule holding aboood40091's FFL decompilation, which
its author publishes without a license, together with this project's changes
to it; it is not covered by this repository's license.
