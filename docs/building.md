# Building

The build turns this source into `Minecraft.Client.rpx`, the game's Wii U
executable, and packages it as a title folder that Cemu runs. Three
commands do the whole job:

```
python tools/prepare.py --base <base game dump> --update <update v688 dump>
python tools/build.py
python tools/package.py --update <update v688 dump>
```

## What you need

- This repository with its submodule. `source/nn/ffl` is a git submodule,
  so clone with
  `git clone --recurse-submodules https://github.com/Jacobwasbeast/mcwiiu-reconstructed.git`,
  or run `git submodule update --init` in an existing clone.
- Your own extracted, decrypted dumps of the base game (title
  `00050000101D9D00`) and of update version 688 (title `0005000E101D9D00`),
  the version this source reconstructs. Each dump is a title folder with
  `code/`, `content/` and `meta/`. DLC dumps are not needed: the pack data
  ships inside the game's content, and the DLC title only unlocks it on a
  console or in Cemu.
- Python 3.8 or newer.
- Docker (Docker Desktop on Windows and macOS) with about 15 GB of
  free space while the toolchain image is first built, and about 6 GB for
  the image and the build after that, plus 1.5 GB for the game files.
  Compiling RedHills needs up to 2 GB of memory per parallel job, so with
  less memory it uses fewer jobs and takes longer; at least 4 GB for Docker
  is recommended (Docker Desktop's resource settings, or WSL's
  `.wslconfig` on Windows).
- Cemu 2.6 or newer to run the result, with the base game in its game list
  or installed in it. A stock Cemu is enough.

## 1. Game files from your own dumps

The build needs two things this repository never contains: the compiled
GX2 shaders and the game's content. `tools/prepare.py` makes both from your
dumps:

```
python tools/prepare.py --base <base game dump> --update <update v688 dump>
```

- `source/platform/wiiu/shaders/` receives the renderer's 31 shader
  objects, the Mii icon module's two shader files, and the data the game
  carries that this repository does not: the key the game logs in to
  Nintendo's online service with, the keys its online monitoring traffic is
  encrypted with, the Shift-JIS conversion tables and a table of decimal
  digits used to print floating-point numbers. The original game built the
  shaders from shader source with Nintendo's tools, and none of this data
  can be written by hand, so all of it is taken from the update's
  `Minecraft.Client.rpx`,
  whose size and SHA-256 are checked first.
  `tools/gamedata/recipe-v688.json` records where each one lives in that
  executable. The files that include them do not compile until
  `prepare.py` has run.
- `assets/` receives `Common/`, `music/` and `WiiU/`: the base game's
  content with the update's files copied over it, about 1.5 GB.
  `assets/.prepared.json` records what was prepared from which inputs.

Both folders are ignored by git. `--shaders-only` regenerates just the
shaders and needs only `--update`.

On PC the launcher's setup imports the same `assets/` without Python; see
`pc-launcher.md`.

## 2. Build the RPX

```
python tools/build.py
```

### The compiler

The game was written for a compiler with its own C++ class layout: where
the virtual table pointer sits, how virtual bases and run-time type
information are laid out, how constructors and destructors are called. The
Wii U's system libraries and the game's own data structures depend on that
layout, so a stock GCC or clang build of this source does not behave like
the game. The build therefore uses
[RedHills](https://github.com/Jacobwasbeast/RedHills), a clang 19 fork that
implements that layout (the Green Hills C++ ABI). The branch used here also
keeps the generated code within what Cemu's recompiler handles correctly.

### What the first run does

1. Downloads the open-source libraries the game uses from their official
   releases into `build/deps/`, and checks each against the hash pinned in
   `tools/build/deps.json`.
2. Builds the toolchain image from `tools/build/Dockerfile`: devkitPro's
   devkitPPC image, pinned by digest, with RedHills compiled from a pinned
   commit and the linker and archiver of the LLVM 22.1.8 release. Compiling
   RedHills takes about half an hour on a 16-core machine; the image is
   built once and reused.
3. Copies `source/`, `tools/` and the libraries into the Docker volume
   `minecraft-wiiu-build`, where compiling is much faster than on a folder
   shared from the host.
4. Builds the C and C++ runtime the game links: newlib 4.6.0 and libstdc++
   from GCC 16.1.0, with devkitPro's patches and the Wii U's two-byte
   `wchar_t` (`tools/redhills/build_runtime16.py`), then the parts of
   libstdc++ and newlib's maths library the game calls again with RedHills.
5. Compiles every `.cpp` under `source/` with RedHills, plus libpng and the
   private copy of zlib the NEX library carries, as the original links them.
   The game's own zlib calls go to the Wii U's `zlib125.rpl`.
6. Links the image the way the original was linked
   (`tools/rpx/build_redhills.py`): the Cafe start-up code, the C runtime
   system layer, the game's own heap, the C++ runtime support, and import
   stubs for every function and variable the game takes from the Wii U's
   system libraries.
7. Checks the image for code Cemu's recompiler is known to mishandle
   (`tools/redhills/crscan.py`), converts it to an RPX
   (`tools/rpx/elf2rpx.py`) and copies `Minecraft.Client.rpx` and its link
   map `Minecraft.Client.map` to `build/`.

The first run takes about 45 minutes on a 16-core machine, most of it
compiling RedHills. Later runs reuse the image and the volume and recompile
only the files that changed, typically in a few minutes.

| Option | Effect |
| --- | --- |
| `--jobs N` | Parallel compile jobs, for the game and for compiling RedHills the first time. By default the game uses every core Docker has, and RedHills as many jobs as fit in the memory Docker has at 2 GB each, up to the core count. Use a lower number if your machine runs hot or out of memory. |
| `--keep-going` | Compile everything that can be compiled and report every failure, instead of stopping at the first. |
| `--clean` | Delete the Docker volume and build from scratch. The toolchain image is kept. |
| `--local` | Build on this machine instead of in Docker (see below). |

`docker volume rm minecraft-wiiu-build` frees the space the build uses
inside Docker, and `docker image rm` with the `minecraft-wiiu-redhills`
image frees the toolchain.

### Building without Docker

`--local` runs the same steps with tools installed on the machine:

- RedHills, built from the `ghs-rtti-vbases` branch of
  https://github.com/Jacobwasbeast/RedHills (see `tools/build/Dockerfile`
  for the commit and the CMake options), with `REDHILLS` pointing at its
  `clang`;
- `ld.lld`, `llvm-ar` and `llvm-nm` from LLVM 22, on `PATH` or in
  `LLVM_BIN`;
- devkitPPC from devkitPro, with `DEVKITPRO` set: its GCC builds the C and
  C++ runtime and its `libgcc` is linked;
- a POSIX shell and `make` for the runtime build, and Python's `capstone`
  module for the image check.

## 3. Run it in Cemu

```
python tools/package.py --update <update v688 dump>
```

writes `build/title/`, a title folder in the layout Cemu loads:

- `code/` - the RPX you built, with `app.xml` and `cos.xml` from the
  update dump;
- `meta/` - the update dump's `meta/` folder;
- `content/` - a link to `assets/` (a junction on Windows, a symbolic link
  elsewhere). `--copy` copies `assets/` instead, for a folder you want to
  move somewhere else.

Start it with

```
Cemu -g build/title/code/Minecraft.Client.rpx
```

Cemu mounts the folder as the update of the base game, which is why the
base game has to be in Cemu's game list or installed in Cemu. Your
installed copy of the game is not modified.

Running on a Wii U console has not been tried with this build yet.

## Third-party code

The game links these libraries, built from their upstream releases. Each
download is checked against a pinned hash, and only the files the build
uses are kept.

| Library | Version | Source | License |
| --- | --- | --- | --- |
| zlib | 1.2.5 | zlib.net | zlib License |
| libpng | 1.6.2 | pnggroup/libpng | libpng License |
| curl | 7.38.0, headers only | curl.se | curl License |
| Boost | 1.53.0, headers only | boost.io | Boost Software License 1.0 |
| stb_image | 2.13 (`ec9db6f`) | nothings/stb | public domain or MIT |
| RapidXml | 1.13 | `tools/build/third_party/` | Boost Software License 1.0 |
| newlib | 4.6.0.20260123 with devkitPro's patch | sourceware.org | BSD-style |
| libstdc++ | GCC 16.1.0 with devkitPro's patch | gnu.org | GPL with the GCC Runtime Library Exception |
| wut | Jacobwasbeast/wut (`31d8e6a`), headers and export lists | GitHub | zlib License |

RapidXml is kept in the repository because its only distribution point
refuses scripted downloads. The game's libpng differs from upstream in
`png.h` and `pngwrite.c`, and curl needs a `curlbuild.h` for the Wii U
target; `tools/build/overrides/` holds those files and explains each
change. curl itself is not compiled: the game calls the Wii U's system
copy in `nlibcurl.rpl`, like every other system library, through the
import stubs the link generates from wut's export lists.
