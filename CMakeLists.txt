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

# The game core, in the C++ dialect it was written in.
file(GLOB_RECURSE LCE_GAME_SOURCES CONFIGURE_DEPENDS "${LCE_SOURCE}/game/*.cpp")
add_library(lce_game OBJECT ${LCE_GAME_SOURCES})
target_link_libraries(lce_game PRIVATE lce_common)
set_target_properties(lce_game PROPERTIES CXX_STANDARD 14)

# The PC platform layer.
file(GLOB_RECURSE LCE_PLATFORM_SOURCES CONFIGURE_DEPENDS "${LCE_SOURCE}/platform/pc/*.cpp")
add_library(lce_platform OBJECT ${LCE_PLATFORM_SOURCES})
target_link_libraries(lce_platform PRIVATE lce_common)
set_target_properties(lce_platform PROPERTIES CXX_STANDARD 17)
if(MSVC)
  # Boost 1.53 still uses std::auto_ptr and std::unary_function, which C++17 removes.
  target_compile_definitions(lce_platform PRIVATE _HAS_AUTO_PTR_ETC=1)
endif()
