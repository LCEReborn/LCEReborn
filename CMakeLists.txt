cmake_minimum_required(VERSION 3.24)

project(LCEReborn LANGUAGES C CXX)

set(LCE_THIRD_PARTY "${CMAKE_SOURCE_DIR}/build/third_party" CACHE PATH
    "Third-party sources laid out by tools/build/deps.py and tools/build/layout.py")

if(NOT EXISTS "${LCE_THIRD_PARTY}/boost/boost")
  message(FATAL_ERROR "Third-party sources not found in ${LCE_THIRD_PARTY}. Run:\n"
                      "  python tools/build/deps.py build/deps\n"
                      "  python tools/build/layout.py build/deps build/third_party")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LCE_SOURCE "${CMAKE_SOURCE_DIR}/source")

# Settings every unit of the PC build shares.
add_library(lce_common INTERFACE)

target_include_directories(lce_common INTERFACE
  "${LCE_SOURCE}"
  "${LCE_THIRD_PARTY}/curl/include"
  "${LCE_THIRD_PARTY}/stb"
  "${LCE_THIRD_PARTY}/zlib"
  "${LCE_THIRD_PARTY}/libpng-wiiu"
  "${LCE_THIRD_PARTY}/libpng"
  "${LCE_THIRD_PARTY}/rapidxml")
target_include_directories(lce_common SYSTEM INTERFACE "${LCE_THIRD_PARTY}/boost")

target_compile_definitions(lce_common INTERFACE
  LCE_PLATFORM_PC=1
  NDEBUG
  BOOST_ENABLE_ASSERT_HANDLER
  BOOST_NO_EXCEPTIONS)

if(MSVC)
  target_compile_definitions(lce_common INTERFACE
    _HAS_EXCEPTIONS=0
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    WIN32_LEAN_AND_MEAN)
  target_compile_options(lce_common INTERFACE /EHs-c- /GR /utf-8 /bigobj)
else()
  target_compile_options(lce_common INTERFACE -fno-exceptions -frtti -fsigned-char)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(lce_common INTERFACE -Wno-c++11-narrowing)
endif()

# SDL3, for the window, input and audio of the PC layer.
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
  URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(SDL3)

# The game core, in the C++ dialect it was written in.
file(GLOB_RECURSE LCE_GAME_SOURCES CONFIGURE_DEPENDS "${LCE_SOURCE}/game/*.cpp")
# Units under game/ that the PC build does not compile here: the platform sound engine (built with the audio
# target below), the Wii U NEX and NQR online stack and the Mii picker.
set(LCE_WIIU_ONLY_GAME_SOURCES
  game/client/audio/PlatformSoundEngineImpl.cpp
  game/core/NS_JobConnectEndPoint_CPP.cpp
  game/core/NS_Job_CPP.cpp
  game/core/NS_SystemChecker_CPP__SystemCheckFunctor.cpp
  game/core/SampleNotificationEventHandler.cpp
  game/network/INQRNetworkManagerListener.cpp
  game/ui/UIScene_AvatarSelectMenu.cpp)
foreach(unit ${LCE_WIIU_ONLY_GAME_SOURCES})
  list(REMOVE_ITEM LCE_GAME_SOURCES "${LCE_SOURCE}/${unit}")
endforeach()
# The vendor units the game core uses that are not Wii U middleware: its own data structures and stb_truetype.
set(LCE_VENDOR_SOURCES
  vendor/_ConnectionProgressParams.cpp
  vendor/_LoadingInputParams.cpp
  vendor/_MapDataMappings.cpp
  vendor/_MapDataMappings_old.cpp
  vendor/_NetworkGameInitData.cpp
  vendor/_Polygon.cpp
  vendor/_SaveListDetails.cpp
  vendor/_SkinAdjustments.cpp
  vendor/_TutorialPopupInfo.cpp
  vendor/_XPARTY_USER_INFO.cpp
  vendor/_XUSER_SIGNIN_INFO.cpp
  vendor/stb_truetype.cpp)
foreach(unit ${LCE_VENDOR_SOURCES})
  list(APPEND LCE_GAME_SOURCES "${LCE_SOURCE}/${unit}")
endforeach()
add_library(lce_game OBJECT ${LCE_GAME_SOURCES})
target_link_libraries(lce_game PRIVATE lce_common)
set_target_properties(lce_game PROPERTIES CXX_STANDARD 14)

# The PC platform layer.
file(GLOB_RECURSE LCE_PLATFORM_SOURCES CONFIGURE_DEPENDS "${LCE_SOURCE}/platform/pc/*.cpp")
list(FILTER LCE_PLATFORM_SOURCES EXCLUDE REGEX "/platform/pc/audio/")
# Units under platform/wiiu shared with PC: the index of the DLC packs in the game content, 4J's memory helpers,
# System (whose clock is switched per platform) and the leaderboard manager (the edition has no leaderboards).
list(APPEND LCE_PLATFORM_SOURCES
  "${LCE_SOURCE}/platform/wiiu/cNintendoDLCInfo.cpp"
  "${LCE_SOURCE}/platform/wiiu/Memory.cpp"
  "${LCE_SOURCE}/platform/wiiu/System.cpp"
  "${LCE_SOURCE}/platform/wiiu/WiiULeaderboardManager.cpp")
add_library(lce_platform OBJECT ${LCE_PLATFORM_SOURCES})
target_link_libraries(lce_platform PRIVATE lce_common SDL3::SDL3-static)
set_target_properties(lce_platform PROPERTIES CXX_STANDARD 17)
if(MSVC)
  # Boost 1.53 still uses std::auto_ptr and std::unary_function, which C++17 removes.
  target_compile_definitions(lce_platform PRIVATE _HAS_AUTO_PTR_ETC=1)
endif()

# Audio: the Miles Sound System and Bink audio the game was built with, the platform sound engine, and the PC
# output driver Miles mixes into (in place of the Wii U AX driver, MilesRadSS.cpp).
file(GLOB LCE_MILES_SOURCES CONFIGURE_DEPENDS "${LCE_SOURCE}/vendor/Miles*.cpp" "${LCE_SOURCE}/vendor/BinkA*.cpp")
list(REMOVE_ITEM LCE_MILES_SOURCES "${LCE_SOURCE}/vendor/MilesRadSS.cpp")
file(GLOB_RECURSE LCE_PC_AUDIO_SOURCES CONFIGURE_DEPENDS "${LCE_SOURCE}/platform/pc/audio/*.cpp")
add_library(lce_audio OBJECT ${LCE_MILES_SOURCES} ${LCE_PC_AUDIO_SOURCES}
  "${LCE_SOURCE}/game/client/audio/PlatformSoundEngineImpl.cpp")
target_link_libraries(lce_audio PRIVATE lce_common SDL3::SDL3-static)
set_target_properties(lce_audio PROPERTIES CXX_STANDARD 14)

# zlib and libpng, the releases the game was built against (libpng with the game's own png.h and pngwrite.c).
set(LCE_ZLIB_SOURCES adler32.c compress.c crc32.c deflate.c infback.c inffast.c inflate.c inftrees.c trees.c
                     uncompr.c zutil.c)
list(TRANSFORM LCE_ZLIB_SOURCES PREPEND "${LCE_THIRD_PARTY}/zlib/")
add_library(lce_zlib STATIC ${LCE_ZLIB_SOURCES})
target_include_directories(lce_zlib PUBLIC "${LCE_THIRD_PARTY}/zlib")

set(LCE_PNG_SOURCES png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c pngrio.c pngrtran.c pngrutil.c pngset.c
                    pngtrans.c pngwio.c pngwtran.c pngwutil.c)
list(TRANSFORM LCE_PNG_SOURCES PREPEND "${LCE_THIRD_PARTY}/libpng/")
add_library(lce_png STATIC ${LCE_PNG_SOURCES} "${LCE_THIRD_PARTY}/libpng-wiiu/pngwrite.c")
target_include_directories(lce_png PUBLIC "${LCE_THIRD_PARTY}/libpng-wiiu" "${LCE_THIRD_PARTY}/libpng")
target_link_libraries(lce_png PUBLIC lce_zlib)

if(MSVC)
  target_compile_definitions(lce_zlib PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_definitions(lce_png PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_executable(LCEReborn $<TARGET_OBJECTS:lce_game> $<TARGET_OBJECTS:lce_platform> $<TARGET_OBJECTS:lce_audio>)
target_link_libraries(LCEReborn PRIVATE lce_png lce_zlib SDL3::SDL3-static)
if(MSVC)
  target_link_options(LCEReborn PRIVATE /errorlimit:0)
endif()
