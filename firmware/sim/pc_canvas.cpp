#include "pc_canvas.h"

#include <cstdlib>

#include "glcdfont.h"

void PcCanvas::fillScreen(uint16_t c) { std::fill(px_.begin(), px_.end(), c); }

void PcCanvas::fillRect(int x, int y, int w, int h, uint16_t c) {
  if (w <= 0 || h <= 0) return;
  int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
  int x1 = x + w > w_ ? w_ : x + w, y1 = y + h > h_ ? h_ : y + h;
  if (x0 >= x1 || y0 >= y1) return;
  if (rot_) {   // a rectangle stays a rectangle: turn two opposite corners onto the glass
    int ax = x0, ay = y0, bx = x1 - 1, by = y1 - 1;
    toGlass(ax, ay);
    toGlass(bx, by);
    x0 = ax < bx ? ax : bx; x1 = (ax < bx ? bx : ax) + 1;
    y0 = ay < by ? ay : by; y1 = (ay < by ? by : ay) + 1;
  }
  for (int yy = y0; yy < y1; yy++) {
    uint16_t *row = &px_[(size_t)yy * w_];
    for (int xx = x0; xx < x1; xx++) row[xx] = c;
  }
}

// Midpoint circle; mask picks quadrants (1 = top-left, 2 = top-right, 4 = bottom-right, 8 = bottom-left)
void PcCanvas::circleQuadrants(int x0, int y0, int r, uint8_t mask, uint16_t c) {
  int f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
  auto plot = [&](int dx, int dy) {
    if (mask & 4) drawPixel(x0 + dx, y0 + dy, c);
    if (mask & 2) drawPixel(x0 + dx, y0 - dy, c);
    if (mask & 8) drawPixel(x0 - dx, y0 + dy, c);
    if (mask & 1) drawPixel(x0 - dx, y0 - dy, c);
  };
  plot(0, r);
  plot(r, 0);
  while (x < y) {
    if (f >= 0) { y--; ddy += 2; f += ddy; }
    x++; ddx += 2; f += ddx;
    plot(x, y);
    plot(y, x);
  }
}

void PcCanvas::drawCircle(int x0, int y0, int r, uint16_t c) { circleQuadrants(x0, y0, r, 0xF, c); }

void PcCanvas::fillCircle(int x0, int y0, int r, uint16_t c) {
  for (int dy = -r; dy <= r; dy++) {
    int dx = 0;
    while ((dx + 1) * (dx + 1) + dy * dy <= r * r) dx++;
    hline(x0 - dx, y0 + dy, 2 * dx + 1, c);
  }
}

void PcCanvas::fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
  int maxr = (w < h ? w : h) / 2;
  if (r > maxr) r = maxr;
  for (int yy = 0; yy < h; yy++) {
    int inset = 0;
    int d = yy < r ? r - yy - 1 : (yy >= h - r ? yy - (h - r) : -1);   // rows inside a corner band
    if (d >= 0) {
      int dx = 0;
      while ((dx + 1) * (dx + 1) + d * d <= r * r) dx++;
      inset = r - dx;
    }
    hline(x + inset, y + yy, w - 2 * inset, c);
  }
}

void PcCanvas::drawRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
  int maxr = (w < h ? w : h) / 2;
  if (r > maxr) r = maxr;
  hline(x + r, y, w - 2 * r, c);
  hline(x + r, y + h - 1, w - 2 * r, c);
  fillRect(x, y + r, 1, h - 2 * r, c);
  fillRect(x + w - 1, y + r, 1, h - 2 * r, c);
  circleQuadrants(x + r, y + r, r, 1, c);
  circleQuadrants(x + w - r - 1, y + r, r, 2, c);
  circleQuadrants(x + w - r - 1, y + h - r - 1, r, 4, c);
  circleQuadrants(x + r, y + h - r - 1, r, 8, c);
}

void PcCanvas::drawLine(int x0, int y0, int x1, int y1, uint16_t c) {   // Bresenham
  int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    drawPixel(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void PcCanvas::print(char ch) {
  if (ch == '\n') { cx_ = 0; cy_ += 8 * textSize_; return; }
  const unsigned char *g = &kGlcdFont[(uint8_t)ch * 5];
  for (int col = 0; col < 5; col++) {
    uint8_t bits = g[col];
    for (int row = 0; row < 8; row++, bits >>= 1)
      if (bits & 1) fillRect(cx_ + col * textSize_, cy_ + row * textSize_, textSize_, textSize_, textColor_);
  }
  cx_ += 6 * textSize_;
}
