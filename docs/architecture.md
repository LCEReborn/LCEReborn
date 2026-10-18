# Architecture

The title is one C++ codebase with a platform abstraction in the middle.
The game core above the seam is platform-pure. The platform backend below
it is the only code that talks to the Cafe OS, the GX2 graphics API, the
VPAD/WPAD controllers and Nintendo's online services. The reconstruction
keeps that shape, and the directory layout is the seam made visible.

## Include root

Every include is written relative to `source/`, for example
`#include "game/world/block/Block.hpp"`. There are no relative includes and
no umbrella headers. A translation unit includes exactly the headers it
uses.

## `source/game/` - the game core

| Directory | Contents |
| --- | --- |
| `game/core` | The engine spine: `Minecraft`, services shared by client and server, `GlStateManager`, achievements and statistics, streams, the console application shell (`CConsoleMinecraftApp`), options, resources, and the many small value classes the rest of the game leans on. |
| `game/util` | Maths and geometry (`AABB`, `Vec3`, `Mth`, `Random`), resource locations, UUIDs and other freestanding helpers. |
| `game/data` | The NBT tag family (`CompoundTag`, `ListTag`, typed tags) and data fixers. |
| `game/world` | `Level`, `LevelSource`, dimensions, explosions, lighting and the save format. |
| `game/world/block` | `Block`, `Blocks`, block states and properties, block entities, materials and `BlockPos`. |
| `game/world/chunk` | Chunk storage, chunk sources, compressed block storage and the region file format. |
| `game/world/biome` | Biomes, biome sources and biome-driven decoration. |
| `game/world/generation` | Chunk generators, structures, features, world templates and the level-type catalogue. |
| `game/entity` | `Entity`, `LivingEntity`, players and the per-kind subdirectories `mob`, `ai` and `projectile`. |
| `game/item` | `Item`, `Items`, `ItemInstance`, inventories, containers, recipes and enchantments. |
| `game/network` | `Packet` and every packet type, packet listeners, connections and the session protocol. |
| `game/server` | The integrated server: chunk cache, command dispatcher, scoreboard, boss events. |
| `game/command` | Console commands and their argument parsing. |
| `game/minigame` | Battle, Tumble and Glide: rule definitions, medals, arenas and minigame state. |
| `game/ui` | The 4J user interface layer (`FJ_*`, `fui*`): screens, controls, HUD, menus and the UI node graph. |
| `game/client/render` | Renderers, particles, models, texture packs and the block/entity render dispatchers. |
| `game/client/audio` | The sound engine front end, music and sound events. |

## `source/platform/wiiu/` - the Nintendo backend

4J's platform layer is a set of `C4J*` and `C*` interfaces implemented once
per console. This directory holds the Wii U implementations: `C4JThread`,
`C4JEvent`, `C4JStorage`, `CStorage`, `CProfile`, `CInput` and the
allocator glue for the Mii face library. These files call Cafe OS and
Nintendo SDK functions directly; no file under `game/` does.

## `source/net/` - multiplayer

The network manager both platforms use, so a Wii U and a PC can play
together: TCP sessions and UDP discovery on port 25570, in one wire format.
It replaces the title's Nintendo Network sessions. `ServerList` keeps the
servers added on the Join tab.

## `source/vendor/` - third-party internals

Internals of libraries the title links against and that the build exposes
with full names: the Miles sound system (`Miles*`), the Bink audio decoder
(`BinkA*`), the game's dlmalloc heap, stb_truetype, the file-local helpers
of the SDK libraries (the `__N_*` files, named after the unit they belonged
to) and small SDK-facing structures. They are kept apart so that game code is never
mistaken for library code.

## `source/nn/` - Nintendo library internals

`source/nn/` holds the smaller SDK libraries the title links: `crypto`,
`dbg`, `erreula` (the error viewer), `save`, `swkbd` (the software keyboard)
and `util`. They are the same category as `source/vendor/`: reconstructed from the symbols and structure
the build exposes, kept apart from the game so the boundary stays explicit,
and to be replaced by a maintained upstream if one ever becomes available.
Includes are spelled from the `source/` root, for example
`#include "nn/dbg/Break.hpp"`.

## `source/nn/ffl/` - the Mii face library (submodule)

The title draws Mii icons with FFL, Nintendo's Face Library, at version
1.5.6. Its source comes from aboood40091's public FFL decompilation
(https://github.com/aboood40091/ffl, version 1.3.10), raised to 1.5.6 by
this reconstruction. That code is not copied into this repository.
`source/nn/ffl` is a git submodule pointing at the `wiiu-recon` branch of
the fork https://github.com/Jacobwasbeast/ffl, where upstream's
`include/nn/ffl/` and `src/` trees are flattened into one directory and the
reconstruction's changes are committed on top. Includes keep the
`<nn/ffl/...>` spelling, and `source/nn/ffl.h` is the umbrella header.

## `source/cafe/` - Cafe SDK pieces

Declarations of the GX2 graphics and GFD shader-file APIs the renderer
uses, and the SDK code the title links statically, rebuilt from the
functions linked into it: the GFD reader, the matrix library (`mtx`,
`mtx44`, `vec`), the C runtime's system layer (`libsys`: time, files,
signals and thread hooks on top of the Cafe OS) and the C++ runtime support
(`libsedg`: `operator new` and `delete`, array construction, run-time type
information and pure-virtual handling). Includes use the SDK spelling, for
example `#include <cafe/gx2.h>`.

## The seam rule

A function belongs below the seam if, and only if, its call graph reaches
an operating-system import. Anything above the seam that calls a platform
function directly is a defect in the reconstruction and is fixed at the seam
rather than kept.
