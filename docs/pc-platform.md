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
`#elif LCE_PLATFORM_PC`. Shared code reaches both builds, so a change to it
is built for both.

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
the license file (see Licenses). `--world` loads a save folder from
`saves/games/` as soon as the main menu is up. Most players start the game
from the launcher (`pc-launcher.md`).

Debug builds (`-DCMAKE_BUILD_TYPE=RelWithDebInfo` or `Debug`) add options for
render testing: `--position` and `--rotation` place the player once a world
has loaded, and `--capture-frames`, `--capture-delay` and `--capture-dir`
save that many frames as PNG files after the delay, then quit.

## Licenses

The PC has no store, so the game reads what is unlocked from `license.cfg`
(or `--license-file`) and writes it on first start with everything locked.
Nothing is unlocked until you edit it, and you should only unlock what you
own. `full_game = true` unlocks the full game, otherwise it runs as the trial.
`dlc` is `none`, `all`, or pack names separated by commas.

## Multiplayer

The PC and Wii U builds share one network manager (`source/net/`), so they
can play together. An online game is hosted on the port set in Online
Settings (25570 unless changed; TCP for players, UDP for finding games on
the local network), and the host is told the port in chat on joining. Online
Settings opens with X (Y on the Wii U) on the Create and Load screens while
Online Game is focused. It also sets whether the world shows on the local network and the
message shown under it in the Join tab (written like a dedicated server's
`motd`); these are saved in `video.txt`. A world started offline can be
opened from the pause menu with Open to LAN. Once open, the same button is
LAN World Settings: it lists the players, changes the message, visibility,
game mode and Host Privileges, and Stop LAN World disconnects the players
who joined over the network. Turning on Host Privileges or Creative asks
first when the world still has achievements. The Join tab lists worlds and
mini games found on the local network, and Add Server saves a name and an
address (`host` or `host:port`) for anything else. Each row shows the game's
icon, its message and the player count, as Java's server list does; a hosted
world's icon is the thumbnail it was last saved with. While the list is
open, each saved server is asked every few seconds for these, and its row
also shows the ping, or that the server can't be reached or runs another
version. The PC keeps the saved servers in `servers.dat` in the save folder;
the Wii U keeps them in its save data. The Mini Games menu's Join tab lists
the same saved servers but only the mini games found on the local network.

`--join <address>` joins a game at start, the same as picking a saved
server.

## Dedicated server

`LCERebornServer` runs a world with no window, sound or local player, on
Windows and Linux. It needs the same content folder as the game and takes
`--content-dir`, `--save-dir` and `--log-dir`. In a release, that content is
what the launcher imported: in the package folder on Windows, where the
server finds it when started there, and in `~/.local/share/LCEReborn/assets`
on Linux. The AppImage only starts the launcher, so run the server from the
tar.gz. The first start writes
`server.properties` to the saves folder; edit it and restart to apply
changes. `world` names the folder in `games` to load, and a new world is made
there if it is missing. Operators may use commands when cheats are allowed.
They are kept by UUID in `ops.json` beside `server.properties`, and `op` and
`deop` change them while the server runs. `op` takes a player's UUID, which
the server logs when they join, or a name. A UUID is better when you know
it: a name of a player who isn't online is held until the first player with
that name joins, who is then kept by their UUID, and anyone can join with
any name.

`motd` is the message under the server in the Join tab. It takes what Java
servers take: `§`, `\u00A7` or `&` formatting codes, `&#RRGGBB` or
`§x§R§R§G§G§B§B` colours, `\n` for a second line, or a JSON text component.
A 64x64 `server-icon.png` beside `server.properties` is the server's icon;
without one, the world's thumbnail is used.

The server reads commands from its standard input: any chat command without
the `/`, plus `save-all` and `stop`. Ctrl+C or SIGTERM also saves and stops,
and with no input it keeps running, so it works as a service or in a
container (publish the TCP port; finding games on the local network doesn't
reach past Docker's own network). The world is saved every
`autosave-minutes`. If it can't start, for example because its port is
taken, it exits with code 1.

## Leaderboards

The console's four boards (Travelling, Mining, Farming and Kills) show one
total each, with no split by difficulty. Leaderboards on the main menu shows
My Statistics: the player's own totals from every game they have played.
Leaderboards in the pause menu shows the current world's board, ranking
everyone who has played there, and X (Y on the Wii U) switches between
Overall, My Score and My Statistics. The world's server counts these and keeps them in the save
by UUID, so a dedicated server keeps one for its players. A game hosted by
an older version doesn't keep one. Stats count in every game mode and world,
as Java's do; only achievements keep the console's limits.

## Discord

With a Discord application ID, the game shows what the player is doing (the
same text as the console's rich presence) in the Discord app running on the
same computer. The ID comes from `--discord-app <id>`, or from the CMake
setting `LCE_DISCORD_APP_ID` when the option is not given. Without either,
nothing is sent.

## Chat

Chat is drawn by the game itself (`game/core/ChatScreen`), so it works the
same on PC and Wii U. Messages are up to 100 characters and show as
`<name> message` for everyone in the game; the last ones fade out above the
hotbar, and the prompt bar shows Chat while other players are in the game.

- **Keyboard:** T opens chat and `/` opens it with a command started. A text
  field opens along the bottom with the history above it. Tab completes
  commands, their arguments and player names, Up and Down choose a
  suggestion or recall sent messages, Page Up and Page Down or the wheel
  scroll the history, Enter sends and Esc closes.
- **Controllers:** D-pad Right opens a panel with two tabs, and Select
  switches between them. Quick Chat sends a phrase, your position or starts a
  reply with A. Keyboard is an on-screen keyboard: A types, X deletes, Y adds
  a space, LT is Shift, LB and RB choose a suggestion, RT completes it and
  Start sends. Pressing the right stick shows the whole chat history across
  the screen; Up and Down scroll it, LT and RT scroll a page and B goes back. On the Wii U GamePad the Keyboard tab opens the GamePad's own
  keyboard instead. While flying, D-pad Right steers and does not open chat.
- **Pause menu:** Y opens chat for any device.

Messages can use Java's formatting codes with `&`: `&0` to `&f` for colors,
`&l` bold, `&o` italic, `&n` underline, `&m` strikethrough, `&k` obfuscated
and `&r` to reset. The server turns them into `§` codes, which the game's
font draws (`Font::draw`).

The Chat button can be changed in Controls for keyboards and controllers.

A message starting with `/` is a command. Each command is a `ChatCommand`
in `game/command/chat/`, registered with `ChatCommands::Register`, which
finds commands by name or alias and checks permission before running them.
They follow Java 1.12, with `/execute`, `/xp` and coordinates as in 1.13:

- **Players and items:** `/give`, `/clear`, `/effect`, `/enchant`, `/xp`,
  `/spawnpoint`, `/gamemode`, `/tp` and `/kill`.
- **World:** `/time`, `/weather`, `/difficulty`, `/defaultgamemode`,
  `/setworldspawn`, `/toggledownfall`, `/seed`, `/summon`, `/setblock` and
  `/fill` (up to 32768 blocks).
- **Chat and server:** `/say`, `/me`, `/msg` (also `/tell` and `/w`),
  `/list`, `/kick`, `/op` and `/deop`.
- **Execute:** `/execute` runs a command as or at other entities: `as`,
  `at`, `positioned` (or `positioned as`), `if entity`, `unless entity` and
  `run`, such as
  `/execute as @e[type=cow] at @s run tp @s ~ ~5 ~`. Messages go to the
  entity running the command, so a mob running one shows nothing.

`/xp add`, `set` and `query` take `points` or `levels`; the older
`/xp 5L` form also works. `/effect` and `/enchant` work on any mob.

`/help` lists the commands you can use a page at a time, and
`/help <command>` shows one usage. `/me`, `/msg`, `/list` and `/help` are
open to everyone. The rest follow the game's own permission rule: the host
and moderators can use them when the world has Host Privileges on, and
anyone else is told they don't have permission. `/kick` can't remove the
host or players on the host's console. `/op` and `/deop` make a player a
moderator or not; the host always is one. In a hosted game they take a
player who is online.

Where a command takes targets, a player name or a selector works
(`TargetSelector`): `@p` nearest player, `@a` all players, `@r` a random
player, `@s` yourself and `@e` all entities. Selectors take Java's
arguments in brackets, such as `@e[type=zombie,r=10]`: `type`, `name`, `m`,
`l`, `lm`, `r`, `rm`, `c`, `x`, `y`, `z`, `dx`, `dy` and `dz`, and `!`
negates `type`, `name` and `m`. Coordinates take `~` for values relative to
where you are, or `^left ^up ^forward` for values relative to where you
face; both are measured from whoever runs the command.
Tab completes item, block, effect, enchantment and entity names, and selector
arguments. The server sends each player the commands they may use over the
`LCE|Commands` channel, again whenever that changes, so completion only
offers those; a game hosted by an older version sends none.

## Debug overlay

A Java-style debug overlay (`game/core/DebugOverlay`) shows position, world,
performance, system and server lines in grouped panels in the top-left
corner. F3 opens it on a keyboard and pressing both sticks together opens it
on a controller, on PC and Wii U.

Settings > More Options > Debug Overlay sets each line to Always, With F3 or
Off, with presets and a live preview (`UIScene_DebugOverlayMenu`). Lines set
to Always show without opening the overlay, which suits the Wii U GamePad.
The choices are saved in each player's profile.

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
and a section from each subsystem that registers one. On Windows,
`minidump.dmp` holds every thread's stack, for Visual Studio or WinDbg with
`LCEReborn.pdb`; Linux has no minidump, and a crash there is caught as a
signal. The renderer's section gives the command buffers' capacity, use and peak, and the
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
The window's title gives the commit and date the game was built from, like
the launcher's (`launcher/BuildInfo.cmake`).

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

On Windows, run CMake from a Visual Studio x64 developer prompt. On Linux,
leave out the compiler options to build with the system's clang or GCC; the
build needs CMake 3.24 or newer, Ninja and the development packages SDL uses
for X11, Wayland and sound (`tools/build/Dockerfile.linux` lists them for
Debian and Ubuntu). The game core is compiled as C++14, the dialect it was
written in; the PC layer as C++17. `tools/build/qt.py` installs the Qt the
launcher needs (`pc-launcher.md`), with ICU on Linux;
`-DLCE_BUILD_LAUNCHER=OFF` builds the game without it.

```
python tools/build_linux.py
```

builds and packages the Linux version in Docker from any system. It builds on
Ubuntu 22.04 so the result also runs on older distributions; the package's
README names the glibc it needs.

```
python tools/package_pc.py
```

packages the build for players as `build/LCEReborn-pc-<commit>.zip` on
Windows or `build/LCEReborn-linux-<commit>.tar.gz` on Linux: the game, the
dedicated server, the launcher with its Qt libraries, `resources/`, a README and the license and
notices of everything in it. On Linux it also packs the same files as
`build/LCEReborn-linux-<commit>.AppImage`, which starts the launcher; that
needs `appimagetool` and the AppImage runtime, which the Docker image has
(`--no-appimage` skips it). It copies only those files, never the game
files, saves or logs in `build/pc`, and refuses a tree with uncommitted
changes, since the package names its commit as its source. License
texts not found in the dependencies themselves are kept in
`tools/package/licenses/`, taken from qtbase and the SPDX license list.

## Releases

Release notes are written under Unreleased in `release/CHANGELOG.md`, a
local file that is not committed. To publish a release, run

```
python tools/release_pc.py
```

where the PC build is made, on a clean tree whose commit is pushed. It
builds, packages, and after asking, publishes a GitHub release named after
the commit and the date it was built (`9b3a482d - 2026-10-19`, tag
`build-9b3a482d`), with the Unreleased notes and the package. It then files
those notes under the release's name. A package of the same commit for the
other system already in `build/` is published with it, so on Windows, run
`tools/build_linux.py` first to release both. When the commit already has a
release, the script only adds its package to it.
An `@name` in the notes, such as the selector `@e`, is published as code so
GitHub doesn't mention that account.
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
| Render Distance | 2 to 32 chunks | 10 | `LevelRenderer::allChanged` sizes the chunk grid (split screen shares the same area, as before); the fog scales with it; each client reports its distance on the `LCE\|ViewDist` payload channel, and the host's server sends and tracks each player that far, up to the host's own (clients that don't report get 10) |
| Simulation Distance | 5 to 32 chunks | 10 | on the host: entities tick within it, random ticks reach one chunk less, and mobs spawn up to two chunks less (at most 8) |
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

## Shader packs

OptiFine and Iris shader packs (OpenGL, as `.zip` files or folders) go in
`shaderpacks/` in the save folder. Settings > More Options > Shaders
(`platform/pc/PcShaderMenu`) picks the pack, changes the global shader
settings and opens the pack's own option screens. The choice and global
settings are saved to `optionsshaders.txt`, and each pack's options to
`shaderpacks/<pack>.txt`, the same files OptiFine uses.

The pack is loaded and run by `platform/pc/shaderpack`; the game reaches it
through `game/client/render/ShaderPackBridge`. Packs see the 1.12 block,
item and entity ids from their `block.properties`, `item.properties` and
`entity.properties`.
