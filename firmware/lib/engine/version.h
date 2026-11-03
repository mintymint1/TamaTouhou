#pragma once
// The one place the game's version lives. The board prints it at boot, the simulator in its
// window title, CMake reads it for the project version, and the release workflow checks that
// a pushed tag (v0.1.0) matches it.
#define TAMA_VERSION "0.1.0"
