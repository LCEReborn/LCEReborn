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
  # Debug information for the crash reports' stacks, without changing the optimised code.
  target_compile_options(lce_common INTERFACE /EHs-c- /GR /utf-8 /bigobj /Z7)
else()
  target_compile_options(lce_common INTERFACE -fno-exceptions -frtti -fsigned-char)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(lce_common INTERFACE -Wno-c++11-narrowing)
endif()

# SDL3, for the window, input and audio of the PC layer.
include(FetchContent)
option(LCE_USE_SYSTEM_SDL "Use an installed SDL3 instead of downloading it" OFF)
if(LCE_USE_SYSTEM_SDL)
  find_package(SDL3 3.4.0 REQUIRED CONFIG)
  set(LCE_SDL_TARGET SDL3::SDL3)
else()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    URL https://www.libsdl.org/release/SDL3-3.4.18.tar.gz
    URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(SDL3)
  get_directory_property(LCE_SDL_WAYLAND DIRECTORY "${SDL3_SOURCE_DIR}" DEFINITION SDL_VIDEO_DRIVER_WAYLAND)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT LCE_SDL_WAYLAND)
    message(FATAL_ERROR "SDL was built without native Wayland support. Install the Wayland, EGL, xkbcommon and wayland-scanner development dependencies, then reconfigure.")
  endif()
  set(LCE_SDL_TARGET SDL3::SDL3-static)
endif()

# Compile the game's shaders once, rather than compiling GLSL at runtime.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(LCE_GLSLC glslc REQUIRED)
add_executable(lce_shader_sources tools/build/shader_sources.cpp source/platform/pc/RenderShaders.cpp)
target_include_directories(lce_shader_sources PRIVATE "${LCE_SOURCE}")
set_target_properties(lce_shader_sources PROPERTIES CXX_STANDARD 17)
set(LCE_SPIRV "${CMAKE_BINARY_DIR}/generated/LceSpirv.hpp")
add_custom_command(OUTPUT "${LCE_SPIRV}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build/spirv.py"
          --generator "$<TARGET_FILE:lce_shader_sources>" --compiler "${LCE_GLSLC}" --output "${LCE_SPIRV}"
  DEPENDS lce_shader_sources tools/build/spirv.py
  VERBATIM)
add_custom_target(lce_shaders DEPENDS "${LCE_SPIRV}")

option(LCE_BUILD_RENDER_TESTS "Build the Vulkan rendering and presentation test" OFF)
if(LCE_BUILD_RENDER_TESTS)
  enable_testing()
  add_executable(lce_vulkan_smoke tests/vulkan_smoke.cpp source/platform/pc/PcGpu.cpp)
  add_dependencies(lce_vulkan_smoke lce_shaders)
  target_include_directories(lce_vulkan_smoke PRIVATE "${LCE_SOURCE}" "${CMAKE_BINARY_DIR}/generated")
  target_link_libraries(lce_vulkan_smoke PRIVATE ${LCE_SDL_TARGET})
  set_target_properties(lce_vulkan_smoke PROPERTIES CXX_STANDARD 17)
  add_test(NAME vulkan_smoke COMMAND lce_vulkan_smoke)
  set_tests_properties(vulkan_smoke PROPERTIES TIMEOUT 30)
  add_executable(lce_platform_text tests/platform_text.cpp
    source/game/core/StringConversion.cpp source/game/core/FileHeader.cpp source/game/core/FileEntry.cpp
    source/platform/wiiu/System.cpp)
  target_link_libraries(lce_platform_text PRIVATE lce_common)
  set_target_properties(lce_platform_text PROPERTIES CXX_STANDARD 14)
  add_test(NAME platform_text COMMAND lce_platform_text)
  add_executable(lce_text_input tests/text_input.cpp source/platform/pc/PcTextInput.cpp)
  target_include_directories(lce_text_input PRIVATE "${LCE_SOURCE}")
  target_link_libraries(lce_text_input PRIVATE ${LCE_SDL_TARGET})
  set_target_properties(lce_text_input PROPERTIES CXX_STANDARD 17)
  add_test(NAME text_input COMMAND lce_text_input)
  add_executable(lce_keyboard_mouse tests/keyboard_mouse.cpp source/platform/pc/KeyboardMouse.cpp)
  target_link_libraries(lce_keyboard_mouse PRIVATE lce_common ${LCE_SDL_TARGET})
  set_target_properties(lce_keyboard_mouse PROPERTIES CXX_STANDARD 17)
  add_test(NAME keyboard_mouse COMMAND lce_keyboard_mouse)
endif()

# The game core, in the C++ dialect it was written in.
file(GLOB_RECURSE LCE_GAME_SOURCES CONFIGURE_DEPENDS "${LCE_SOURCE}/game/*.cpp")
# Units under game/ that the PC build does not compile: the Miles sound engine and the game's glue to it (PC audio
# is platform/pc/audio), the Wii U NEX and NQR online stack and the Mii picker.
set(LCE_WIIU_ONLY_GAME_SOURCES
  game/client/audio/EventSystem.cpp
  game/client/audio/FalloffGraph.cpp
  game/client/audio/PlatformSoundEngineImpl.cpp
  game/core/PODHandleArray.cpp
  game/util/LegacyUtilityFunctions.cpp
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
# The LAN and internet play shared with the Wii U build.
file(GLOB LCE_NET_SOURCES CONFIGURE_DEPENDS "${LCE_SOURCE}/net/*.cpp")
list(APPEND LCE_PLATFORM_SOURCES ${LCE_NET_SOURCES})
# Units under platform/wiiu shared with PC: the index of the DLC packs in the game content, 4J's memory helpers,
# System (whose clock is switched per platform) and the leaderboard manager (the edition has no leaderboards).
list(APPEND LCE_PLATFORM_SOURCES
  "${LCE_SOURCE}/platform/wiiu/cNintendoDLCInfo.cpp"
  "${LCE_SOURCE}/platform/wiiu/Memory.cpp"
  "${LCE_SOURCE}/platform/wiiu/System.cpp"
  "${LCE_SOURCE}/platform/wiiu/WiiULeaderboardManager.cpp")
add_library(lce_platform OBJECT ${LCE_PLATFORM_SOURCES})
add_dependencies(lce_platform lce_shaders)
target_include_directories(lce_platform PRIVATE "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(lce_platform PRIVATE lce_common ${LCE_SDL_TARGET})
set_target_properties(lce_platform PROPERTIES CXX_STANDARD 17)
add_custom_target(lce_build_info
  COMMAND "${CMAKE_COMMAND}" -DSOURCE_DIR="${CMAKE_SOURCE_DIR}" -DOUTPUT="${CMAKE_BINARY_DIR}/generated/BuildInfo.hpp"
          -P "${CMAKE_SOURCE_DIR}/launcher/BuildInfo.cmake"
  BYPRODUCTS "${CMAKE_BINARY_DIR}/generated/BuildInfo.hpp")
add_dependencies(lce_platform lce_build_info)
target_include_directories(lce_platform PRIVATE "${CMAKE_BINARY_DIR}/generated")
if(MSVC)
  # Boost 1.53 still uses std::auto_ptr and std::unary_function, which C++17 removes.
  target_compile_definitions(lce_platform PRIVATE _HAS_AUTO_PTR_ETC=1)
endif()

# Audio: the game's sound bank and Bink Audio, played through SDL.
file(GLOB_RECURSE LCE_PC_AUDIO_SOURCES CONFIGURE_DEPENDS "${LCE_SOURCE}/platform/pc/audio/*.cpp")
add_library(lce_audio OBJECT ${LCE_PC_AUDIO_SOURCES})
target_link_libraries(lce_audio PRIVATE lce_common ${LCE_SDL_TARGET})
set_target_properties(lce_audio PROPERTIES CXX_STANDARD 17)

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
find_package(Threads REQUIRED)
target_link_libraries(LCEReborn PRIVATE lce_png lce_zlib ${LCE_SDL_TARGET} Threads::Threads)
if(WIN32)
  target_link_libraries(LCEReborn PRIVATE shell32 ws2_32 dbghelp)
  # The executable's icon (tools/pcart/make_icon.py).
  set(LCE_ICON "${CMAKE_SOURCE_DIR}/resources/pc/icon.ico")
  file(WRITE "${CMAKE_BINARY_DIR}/LCEReborn.rc" "1 ICON \"${LCE_ICON}\"\n")
  target_sources(LCEReborn PRIVATE "${CMAKE_BINARY_DIR}/LCEReborn.rc")
  set_source_files_properties("${CMAKE_BINARY_DIR}/LCEReborn.rc" PROPERTIES OBJECT_DEPENDS "${LCE_ICON}")
endif()
# The PC interface art (tools/pcart) is read from resources/ in the working folder unless --resource-dir is given.
add_custom_command(TARGET LCEReborn POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/resources/pc"
          "$<TARGET_FILE_DIR:LCEReborn>/resources")
if(MSVC)
  target_link_options(LCEReborn PRIVATE /errorlimit:0 /DEBUG /OPT:REF /OPT:ICF)
endif()

function(lce_desktop_entry target name icon)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build/desktop.py"
        --executable "$<TARGET_FILE:${target}>" --name "${name}"
        --working-dir "${CMAKE_SOURCE_DIR}" --icon "${CMAKE_SOURCE_DIR}/${icon}"
        --output "${CMAKE_BINARY_DIR}/org.lcereborn.${target}.desktop"
      VERBATIM)
  endif()
endfunction()
lce_desktop_entry(LCEReborn "LCEReborn" resources/pc/icon.png)

option(LCE_BUILD_LAUNCHER "Build the Qt launcher (needs python tools/build/qt.py first)" ON)
if(LCE_BUILD_LAUNCHER)
  add_subdirectory(launcher)
endif()
