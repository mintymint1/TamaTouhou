#pragma once
// CO5300 466x466 AMOLED over QSPI with a full framebuffer (Arduino_Canvas) in PSRAM.
// Every frame repaints the whole scene and flush()es it (~46 ms at 80 MHz, the fps limit).
// Never fillScreen(BLACK) before drawing: a flush still reading the buffer would show black.
#include <Arduino_GFX_Library.h>

namespace hw {

extern Arduino_Canvas *gfx;   // draw into this, then flush()

bool displayBegin();          // after powerBegin() (BLDO1 must be on)
void setBrightness(uint8_t level);  // 0..255; 0 = off (and skip rendering)

}  // namespace hw
