#pragma once
// Software framebuffer with the subset of Arduino_GFX's API the app layer uses, so the same
// screen code draws on the PC. RGB565 like the board, so colours match exactly.
#include <cstdint>
#include <functional>
#include <vector>

class PcCanvas {
 public:
  PcCanvas(int w, int h) : w_(w), h_(h), px_((size_t)w * h, 0) {}

  void fillScreen(uint16_t c);
  void fillRect(int x, int y, int w, int h, uint16_t c);
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c);
  void drawRoundRect(int x, int y, int w, int h, int r, uint16_t c);
  void fillCircle(int x0, int y0, int r, uint16_t c);
  void drawCircle(int x0, int y0, int r, uint16_t c);
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c);
  void drawPixel(int x, int y, uint16_t c) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    toGlass(x, y);
    px_[(size_t)y * w_ + x] = c;
  }

  // Quarter turns clockwise, like Arduino_Canvas::setRotation on the board (Settings: rotate
  // screen). Drawing calls stay in screen coordinates; pixels() is what the glass shows.
  void setRotation(uint8_t r) { rot_ = r & 3; }

  // classic 5x7 font, transparent background (like setTextColor(c) on the board)
  void setTextSize(uint8_t s) { textSize_ = s ? s : 1; }
  void setTextColor(uint16_t c) { textColor_ = c; }
  void setCursor(int x, int y) { cx_ = x; cy_ = y; }
  void print(const char *s) { while (*s) print(*s++); }
  void print(char c);

  void flush() { if (onFlush) onFlush(); }   // the board DMAs to the panel; the PC presents

  int width() const { return w_; }
  int height() const { return h_; }
  const uint16_t *pixels() const { return px_.data(); }
  std::function<void()> onFlush;

 private:
  void hline(int x, int y, int w, uint16_t c) { fillRect(x, y, w, 1, c); }
  void toGlass(int &x, int &y) const {   // screen -> pixels(), the board canvas's mapping
    const int m = w_ - 1, sx = x, sy = y;
    if (rot_ == 1) { x = m - sy; y = sx; }
    else if (rot_ == 2) { x = m - sx; y = m - sy; }
    else if (rot_ == 3) { x = sy; y = m - sx; }
  }
  void circleQuadrants(int x0, int y0, int r, uint8_t mask, uint16_t c);
  int w_, h_;
  std::vector<uint16_t> px_;
  uint8_t rot_ = 0;
  uint8_t textSize_ = 1;
  uint16_t textColor_ = 0xFFFF;
  int cx_ = 0, cy_ = 0;
};
