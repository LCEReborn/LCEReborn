# PC platform layer

LCEReborn builds the reconstructed game for more than the Wii U. The game
core under `source/game/` is shared; each platform supplies its own layer
under `source/platform/<platform>/`. The PC layer is native: it does not
emulate the Wii U or Nintendo's services, it implements what the game asks
of its platform with ordinary PC facilities.

## Selecting the platform

`source/platform/Platform.hpp` defines exactly one of:

| Macro | When |
| --- | --- |
| `LCE_PLATFORM_WIIU` | the Wii U build (`__WIIU__`, set by `tools/rpx/build_redhills.py`) |
| `LCE_PLATFORM_PC` | Windows, Linux or macOS compilers |

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
data, Nintendo friend presence, Pia) are declared under that platform's
`#if`, and so are the game's calls to them.

## Files and folders on PC

Game content is read from `assets/` (what `tools/prepare.py` writes) and
saves go to `saves/`, both relative to the working directory. The
`LCE_CONTENT_DIR` and `LCE_SAVE_DIR` environment variables override them
(`platform/pc/PcPaths`).

## Building on PC

```
python tools/build/deps.py build/deps
python tools/build/layout.py build/deps build/third_party
cmake -S . -B build/pc -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build/pc
```

Run CMake from a Visual Studio x64 developer prompt. The game core is
compiled as C++14, the dialect it was written in; the PC layer as C++17.
