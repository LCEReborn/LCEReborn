# PC platform layer

LCEReborn builds the reconstructed game for more than the Wii U. The game
core under `source/game/` is shared; each platform supplies its own layer
under `source/platform/<platform>/`. The PC layer is native: it does not
emulate the Wii U or Nintendo's services, it implements what the game asks
of its platform with ordinary PC facilities.

## Selecting the platform

Each build defines exactly one platform macro on the compiler command line:

| Macro | Defined by |
| --- | --- |
| `LCE_PLATFORM_WIIU` | the Wii U build (`tools/rpx/build_redhills.py`) |
| `LCE_PLATFORM_PC` | the PC build (`CMakeLists.txt`) |

`source/platform/Platform.hpp` checks that exactly one is set, and picks one
from the compiler when neither is (`__WIIU__` for the Wii U; Windows, Linux or
macOS for PC).

Code that differs between platforms uses `#if LCE_PLATFORM_WIIU` /
`#elif LCE_PLATFORM_PC`. The Wii U build must keep producing the same RPX:
every change is checked by building it and comparing the bytes.

## How the game reaches its platform

4J wrote the game against a small set of interfaces and a Win32-style API
(from its Xbox 360 roots). The Wii U layer implements both over Cafe OS; the
PC layer implements them natively.

| Piece | Shared header | Wii U | PC |
| --- | --- | --- | --- |
| Profile (players, sign-in, awards) | `platform/C4JProfile.hpp` (`C_4JProfile`) | `wiiu/CProfile` | `pc/CProfile` |
| Storage (saves, options, DLC) | `platform/C4JStorage.hpp` (`C4JStorage`) | `wiiu/CStorage` | `pc/CStorage` |
| Input | `platform/C4JInput.hpp` (`C_4JInput`) | `wiiu/CInput` | `pc/CInput` |
| Threads and events | `platform/C4JThread.hpp`, `C4JEvent.hpp`, `C4JEventArray.hpp`, `C4JEventQueue.hpp` | `wiiu/C4J*Impl` | `pc/C4J*Impl` |
| Locks | `game/core/CriticalSection.hpp` (`C4JCriticalSection`) | `OSFastMutex` | recursive mutex |
| Win32-style runtime (time, memory) | `platform/Runtime.hpp` | `wiiu/WiiU_Runtime` | `pc/Runtime` |
| Win32-style files | `platform/FileApi.hpp`, `platform/FileSystem.hpp` | `wiiu/WiiU_FileCache`, `wiiu/NintendoFileSystem_WiiU` | `pc/FileApi` |
| Thread-local storage | `platform/TLSStorage.hpp` | `wiiu/TLSStorageCafe` | `pc/TLSStorage` |
| Timing | `platform/PlatformTiming.hpp` | `wiiu/PlatformTiming` | `pc/PlatformTiming` |

Headers named after a platform class (`platform/CProfile.hpp`,
`platform/Runtime.hpp`, ...) only select the platform's header; game code
includes those and never `platform/wiiu/...` or `platform/pc/...` directly.

Members of a shared interface whose types exist only on one platform (Mii
data, Nintendo friend presence) are declared under that platform's
`#if`, and so are the game's calls to them.

## Files and folders on PC

Game content is read from `assets/` (what the launcher's setup or
`tools/prepare.py` writes), saves
go to `saves/` and the PC interface art is read from `resources/`, all
relative to the working directory. The `--content-dir`, `--save-dir` and
`--resource-dir` arguments override them (`platform/pc/PcPaths`). A resource
folder with a `pc` subfolder, such as the repository's own `resources/`, is
read from that subfolder.

The game reads no environment variables for its own settings; everything
comes from its arguments (`platform/pc/PcLaunchOptions`, `--help` lists them).
`--username` sets the player name, falling back to the OS user name, and
`--language` the language, falling back to the system's. `--uuid` sets the
first player's identity, 32 hexadecimal digits with or without dashes;
without it the identity is made from the player name. `--license-file` sets
the license file (see Licenses). Most players start the game from the
launcher (`pc-launcher.md`).

## Licenses

The PC has no store, so the game reads what is unlocked from `license.cfg`
(or `--license-file`) and writes it on first start with everything locked.
Nothing is unlocked until you edit it, and you should only unlock what you
own. `full_game = true` unlocks the full game, otherwise it runs as the trial.
`dlc` is `none`, `all`, or pack names separated by commas.

## Multiplayer

The PC and Wii U builds share one network manager (`source/net/`), so they
can play together. An online game is hosted on port 25570 (TCP for players,
UDP for finding games on the local network). The Join tab lists worlds and
mini games found on the local network, and Add Server saves a name and an
address (`host` or `host:port`) for anything else. While the list is open,
each saved server is asked every few seconds what it is hosting, and its row
shows that world or mini game. The PC keeps the saved servers in
`servers.dat` in the save folder; the Wii U keeps them in its save data. The
Mini Games menu's Join tab lists the same saved servers but only the mini
games found on the local network.

## Logging

The game logs through `platform/pc/Log`. Each file declares a named logger
(`static const Logger sLog("Renderer");`) and writes with `LOG_INFO(sLog, ...)`
and the other level macros, which take printf-style arguments. Every line has
the same parseable form:

```
[14:01:36.665] [(4J) Main thread/INFO] [Renderer]: NVIDIA Corporation / ...
```

All levels from DEBUG up are written, both to standard output and to
`logs/latest.log` (`--log-dir` moves the folder). The game doesn't filter
anything: a launcher reading the output does that itself. TRACE calls are
compiled out unless the build defines `LCE_LOG_MIN_LEVEL=0`. At startup the
previous `latest.log` is renamed by its date, as Java Edition does
(`2026-10-17-1.log`, then `-2`, ...), and gzipped on a background thread.

Callers only format the message; a writer thread adds the time, thread and
level and writes in batches, so logging doesn't wait on the console or the
disk. ERROR and FATAL wait until their line is on disk, so it survives a crash
right after. `CProfile::DebugPrintf` and fatal load errors go through the
same logger.

## Crash reports

A fatal renderer error or an unhandled crash writes a folder to
`crash-reports/` beside the logs folder (`platform/pc/PcCrashReport`). Its
`report.txt` has the reason, the failing thread's stack, every thread by name
and a section from each subsystem that registers one; `minidump.dmp` holds
every thread's stack, for Visual Studio or WinDbg with `LCEReborn.pdb`. The
renderer's section gives the command buffers' capacity, use and peak, and the
log warns once when they pass 75% of capacity. The capacity fits the largest
render distance.

## Window and display

The game draws into a resizable window at the window's own resolution and
shape. The world fills the window, its field of view following the window's
proportions. The menus and the HUD are the Wii U's 720p, 16:9 interface: they
are drawn in the largest 16:9 area centred in the window, scaled to it, and
full-screen backgrounds such as the title panorama, the dimming behind menus and
solid fills that cover a whole menu are scaled to cover the whole window
(`Renderer::SetLayout`). Pixel art in the interface is sampled so it stays
even at any scale, not only whole multiples of 720p. The mouse is mapped into
the same area.
F11 or Alt+Enter switches between the window and borderless fullscreen.
Whether the game is fullscreen and whether it waits for the monitor's refresh
(V-Sync) are kept in `display.txt` in the save folder (`platform/pc/PcWindow`).

The game runs one frame at a time at 60 frames per second, like the Wii U: the
menus and their animations advance one step per frame, so the main loop holds
the frame rate at 60 (`PcWindowPaceFrame`) whatever the monitor's refresh rate
or the V-Sync setting.

## Sound

The PC build plays the game's own sound bank (`WiiU/Sound/Minecraft.msscmp`)
and music (`.binka` files) with its own engine in `platform/pc/audio`, in
place of the Miles Sound System the Wii U build uses:

- `PcSoundBank` reads the bank's events and the Bink Audio files inside it.
- `BinkAudioDecoder` decodes Bink Audio. It is ported from FFmpeg.
- `PcMixer` mixes the voices to stereo through SDL.
- `PcSoundEngine` is the game's `PlatformSoundEngine`.
  - Events pick their sounds the way the bank says: weighted, never the same
    twice in a row, or in turn.
  - Each sound keeps its instance limit.
  - A sound in the world is placed and faded with the distance as on the
    Wii U.
  - The copy of each sound the game sends to the GamePad is kept silent.

## Building on PC

```
python tools/build/deps.py build/deps
python tools/build/layout.py build/deps build/third_party
python tools/build/qt.py
cmake -S . -B build/pc -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build/pc
```

Run CMake from a Visual Studio x64 developer prompt. The game core is
compiled as C++14, the dialect it was written in; the PC layer as C++17.
`tools/build/qt.py` installs the Qt the launcher needs (`pc-launcher.md`);
`-DLCE_BUILD_LAUNCHER=OFF` builds the game without it.

```
python tools/package_pc.py
```

packages the build for players as `build/LCEReborn-pc-<commit>.zip`: the
game, the launcher with its Qt libraries, `resources/`, a README and the
license and notices of everything in it. It copies only those files, never
the game files, saves or logs in `build/pc`, and refuses a tree with
uncommitted changes, since the zip names its commit as its source. License
texts not found in the dependencies themselves are kept in
`tools/package/licenses/`, taken from qtbase and the SPDX license list.

## Releases

Release notes are written under Unreleased in `release/CHANGELOG.md`, a
local file that is not committed. To publish a release, run

```
python tools/release_pc.py
```

from the same developer prompt, on a clean tree whose commit is pushed. It
builds, packages, and after asking, publishes a GitHub release named after
the commit and the date it was built (`9b3a482d - 2026-10-19`, tag
`build-9b3a482d`), with the Unreleased notes and the zip. It then files those
notes under the release's name.
`--dry-run` shows the release without publishing it, and `--prerelease`
marks it as a pre-release. The launcher's News tab shows the published
releases.

## Custom menus

New menus are built from the game's own interface, so they look and behave
like the rest of it (focus, sounds, prompts, controller, keyboard, mouse and
touch). `UIScene_CustomMenu` (scene `EUIScene_CustomMenu`) loads the game's
list menu movie, the one its settings lists use, and fills it from a page: a
`UICustomMenuPage` subclass whose `Build` adds section labels, buttons,
checkboxes, sliders and label/value rows, each with an optional description,
and whose `OnPress`, `OnToggle` and `OnSlide` handle them. `OpenPage` opens
another page on top. Settings > More Options opens the first page,
`UIMoreOptionsPage`, with the display and video settings.

## Video settings

`game/core/VideoSettings` holds the settings the console fixed, and saves
them to `video.txt` in the save folder whenever one changes (the PC build
loads the file at startup; a platform that never loads it keeps the
defaults, which are the original game's values).

| Setting | Range | Default | What follows it |
| --- | --- | --- | --- |
| Render Distance | 2 to 32 chunks | 10 | `LevelRenderer::allChanged` sizes the chunk grid (split screen shares the same area, as before); the fog scales with it; the host's server sends chunks that far (`PlayerChunkMap` accepts 2 to 32) |
| FOV | 30 to 110 degrees, "Normal" at 70, "Quake Pro" at 110 | 70 | `GameRenderer::getFov` |
| Fancy Graphics | on/off | on | `Options::mFancyGraphics`: leaves, shadows and item detail; the world is rebuilt when it changes |
| Smooth Lighting | on/off | on | `Options::mAmbientOcclusion`; the world is rebuilt |
| Particles | All, Decreased, Minimal | All | `Options::mParticles` |
| Biome Blend | Off, 3x3 up to 15x15 | 3x3 | the averaging radius in `BiomeColors::getAverageColor`; the world is rebuilt |

Leaves pick their fast or fancy texture from the leaf block itself, so
chunks built on any thread agree after a change.

The list stands alone in the middle of the screen like the game's settings
menus. When the highlighted row has a description, the Show Description
prompt (the game's X/Square action, X on the keyboard) slides the list to the
left and the description panel out from behind it, sized to its text; the
same button tucks it away again. The panel follows the highlight while open.
