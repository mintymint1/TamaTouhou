#pragma once
// The board side of app_platform.h (plat::* in platform_esp32.cpp), plus what main.cpp needs
// from it at boot.
#include <cstdint>

bool assetsBegin();                 // mount LittleFS (theme art from firmware/data/)
uint8_t platformBrightnessLevel();  // the Settings choice: 0 auto, 1 low, 2 mid, 3 high
void rotationBegin();               // after displayBegin(): turn the canvas the stored way
void rotationFlush();               // store a changed rotation (an NVS write: call when idle)
