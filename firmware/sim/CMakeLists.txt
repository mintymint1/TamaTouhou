# The PC simulator: the real game (lib/engine + lib/app + the theme tables) in an SDL3 window.
# Normally driven by the repo's Makefile (`make run`); see docs/development.md.
#
# Portable: macOS, Linux, and Windows (MSYS2/MinGW or MSVC).
cmake_minimum_required(VERSION 3.20)

get_filename_component(FW "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
file(STRINGS "${FW}/lib/engine/version.h" TAMA_VERSION_LINE REGEX "#define TAMA_VERSION")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" TAMA_VERSION "${TAMA_VERSION_LINE}")
project(tamatouhou_sim VERSION ${TAMA_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# --------------------------------------------------------------------------- SDL3
# A pinned release, checked by hash and linked statically, so nobody needs SDL installed. The
# tarball is kept in firmware/.deps so a clean rebuild doesn't download it again.
include(FetchContent)
if(APPLE)   # SDL has per-platform files that are empty on macOS: index the archive once,
            # quietly (ar's own indexing pass warns about every empty file)
  foreach(lang C CXX OBJC OBJCXX)
    set(CMAKE_${lang}_ARCHIVE_CREATE "<CMAKE_AR> qcS <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_${lang}_ARCHIVE_APPEND "<CMAKE_AR> qS <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_${lang}_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
  endforeach()
endif()
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
  URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
  DOWNLOAD_DIR "${FW}/.deps"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(SDL3)

# --------------------------------------------------------------------------- the game
# Engine + screens + generated theme tables (all shared with the board) + the PC platform.
file(GLOB APP_SOURCES CONFIGURE_DEPENDS "${FW}/lib/app/*.cpp")
file(GLOB ENGINE_SOURCES CONFIGURE_DEPENDS "${FW}/lib/engine/*.cpp")
add_executable(tamatouhou-sim
  main.cpp game.cpp platform_pc.cpp pc_canvas.cpp save_file.cpp
  "${FW}/lib/theme_gen/theme_data.cpp"
  ${ENGINE_SOURCES} ${APP_SOURCES})
target_include_directories(tamatouhou-sim PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}" "${FW}/lib/engine" "${FW}/lib/theme_gen" "${FW}/lib/app")
target_link_libraries(tamatouhou-sim PRIVATE SDL3::SDL3)
target_compile_options(tamatouhou-sim PRIVATE
  $<IF:$<CXX_COMPILER_ID:MSVC>,/W3,-Wall -Wextra -Wno-unused-parameter>)
target_compile_definitions(tamatouhou-sim PRIVATE DATA_DIR="${FW}/data" SAVE_DIR="${FW}/sim/save")
if(MINGW)   # no libstdc++/libgcc/winpthread DLLs next to the .exe
  target_link_options(tamatouhou-sim PRIVATE -static -static-libgcc -static-libstdc++)
endif()
