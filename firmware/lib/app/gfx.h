#pragma once
// The drawing surface the screens paint into: Arduino_GFX's full-frame canvas on the board,
// a software framebuffer with the same method names on the PC (sim/pc_canvas.h).
// Only the subset used by the app layer is required: fillScreen, fillRect, fillRoundRect,
// drawRoundRect, fillCircle, drawCircle, drawLine, setTextSize/Color/Cursor, print, flush.
#ifdef ARDUINO
#include <Arduino_GFX_Library.h>
using Gfx = Arduino_Canvas;
#else
#include "pc_canvas.h"
using Gfx = PcCanvas;
#endif

namespace app {
extern Gfx *gfx;
}

// Screen geometry (466x466 round AMOLED)
constexpr int kScreenW = 466;
constexpr int kScreenH = 466;
constexpr int CX = 233;
constexpr int CY = 233;

// A point on the glass (the panel's own pixels: touch, mouse) to screen coordinates when the
// canvas is rotated by `quarterTurns` clockwise (plat::rotation()). The inverse of the
// canvas's mapping: Arduino_Canvas::writePixelPreclipped on the board, PcCanvas on the PC.
template <typename T>
inline void glassToScreen(uint8_t quarterTurns, T &x, T &y) {
  const T m = (T)(kScreenW - 1), gx = x, gy = y;
  switch (quarterTurns & 3) {
    case 1: x = gy; y = (T)(m - gx); break;
    case 2: x = (T)(m - gx); y = (T)(m - gy); break;
    case 3: x = (T)(m - gy); y = gx; break;
    default: break;
  }
}
