# Overview

This repository holds a reconstructed C++ source tree for Minecraft: Wii U
Edition, the 4J Studios Legacy Console Edition codebase as it shipped on
Wii U. It is a matching decompilation: the source is written by reading the
original Wii U executable's compiled code, together with the symbols, type
information and string tables the unstripped build carries, and each
function is compiled and compared with the original until it matches. The
Xbox 360 TU75 build serves as a behavioural cross-check. Every file is
hand-authored text; no decompiler output, disassembly or game data is
committed. See `provenance.md`.

## Who is writing it

All of the code in this repository is AI-constructed. No human writes
source here; people direct the work, set the rules in `conventions.md`, and
review.

The work is coordinated by Claude, which hands Claude Opus 5.5 worker agents
one task at a time with the evidence for it: a class family to rebuild, or a
set of functions whose compiled code differs from the original. Each change
is compiled and compared function by function with the original executable,
and independently audited, before it is accepted. Earlier phases worked the
same way with other agents: the dispatchers Sol and Fable handed class
families to Luna and Opus 5 workers.

## What is here

- `source/game/` - the platform-independent game: world, blocks, entities,
  items, networking, UI, rendering and audio.
- `source/platform/wiiu/` - the Nintendo backend of 4J's platform layer
  (threads, storage, profiles, input, the Mii icon module).
- `source/net/` - multiplayer shared by the Wii U and PC builds, so the two
  can play together.
- `source/vendor/` - reconstructed internals of third-party libraries the
  title links against (Miles audio, Bink audio, dlmalloc, stb_truetype).
- `source/nn/` - the small SDK libraries the title links (crypto, software
  keyboard, error viewer), plus the Mii face library in the `source/nn/ffl`
  submodule.
- `source/cafe/` - reconstructed Cafe SDK pieces: GX2 and GFD declarations,
  the matrix library, the C runtime system layer and the C++ runtime support
  the title links.
- `docs/` - these notes.
- `tools/` - `prepare.py` sets up the game files from your own dumps,
  `build.py` builds the RPX with RedHills, and `package.py` lays it out as a
  title folder for Cemu. `tools/rpx/` and `tools/redhills/` link the image
  the way the original was linked; `tools/build/` holds the pinned library
  list, the toolchain image and the few altered library files;
  `tools/measure/` compares a build with the original executable.
- `launcher/` - the PC port's Qt launcher, which imports the game files
  without Python, keeps profiles and shows the game's log (`pc-launcher.md`).

## What is not here

No game assets, textures, models, sounds, fonts, save data, executables,
compiled shaders, symbol tables, disassembly, or decompiler output are
committed to this repository, and none should ever be.

## Status

`tools/build.py` compiles the whole tree and links `Minecraft.Client.rpx`
without any code from the original executable. It runs in a stock Cemu 2.6:
it boots to the menus, creates and loads worlds, and its server has run at
the full 20 ticks per second in testing. It has not been tried on a Wii U
console. Nintendo ended the Wii U's online services in April 2024, and the
online features have not been tested.

The reconstruction is still being refined toward the original function by
function, and a behaviour difference you find is a bug worth reporting.
`tools/measure/parity.py` compares your build with your copy of the
original executable function by function; see `docs/measuring.md` for what
it checks and the current result.
