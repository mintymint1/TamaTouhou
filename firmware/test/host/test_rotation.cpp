// Settings > Rotate screen: the canvas turns what it draws, and glassToScreen() (gfx.h) turns
// touches back. PcCanvas stands in for the board's Arduino_Canvas, whose mapping is copied
// below from its writePixelPreclipped() so the two can't drift apart unnoticed.
#include "framework.h"
#include "gfx.h"

namespace {
constexpr int N = kScreenW, M = kScreenW - 1;

// Where Arduino_Canvas (GFX Library for Arduino 1.6.x) puts screen pixel (x, y) in its
// framebuffer, i.e. on the glass, for each rotation.
void boardGlass(int r, int x, int y, int &gx, int &gy) {
  switch (r) {
    case 1: gy = x; gx = M - y; break;   // fb += x * height + (max_y - y)
    case 2: gy = M - y; gx = M - x; break;
    case 3: gy = M - x; gx = y; break;   // fb += (max_x - x) * height + y
    default: gx = x; gy = y; break;
  }
}

// The one lit pixel on an otherwise black canvas
bool litPixel(const PcCanvas &c, int &gx, int &gy) {
  int found = 0;
  for (int i = 0; i < N * N; i++)
    if (c.pixels()[i]) { gx = i % N; gy = i / N; found++; }
  return found == 1;
}

const int kPoints[][2] = {{0, 0}, {M, 0}, {0, M}, {M, M}, {CX, CY}, {CX, 0}, {94, 330}, {372, 102}};
}  // namespace

TEST(rotation_canvas_matches_board_and_touch_undoes_it) {
  for (int r = 0; r < 4; r++)
    for (const auto &pt : kPoints) {
      PcCanvas c(N, N);
      c.setRotation((uint8_t)r);
      c.drawPixel(pt[0], pt[1], 0xFFFF);
      int gx = -1, gy = -1, bx = -2, by = -2;
      CHECK(litPixel(c, gx, gy));
      boardGlass(r, pt[0], pt[1], bx, by);
      CHECK_EQ(gx, bx);   // same place as on the board
      CHECK_EQ(gy, by);
      glassToScreen((uint8_t)r, gx, gy);   // a finger on that pixel taps the drawn point
      CHECK_EQ(gx, pt[0]);
      CHECK_EQ(gy, pt[1]);
    }
}

TEST(rotation_quarter_turn_is_clockwise) {
  PcCanvas c(N, N);
  c.setRotation(1);
  c.drawPixel(CX, 0, 0xFFFF);   // top of the picture...
  int gx = -1, gy = -1;
  CHECK(litPixel(c, gx, gy));
  CHECK_EQ(gx, M);              // ...ends up on the right edge of the glass
  CHECK_EQ(gy, CY);
}

TEST(rotation_fill_rect_covers_the_same_pixels_as_plotting) {
  for (int r = 1; r < 4; r++) {
    PcCanvas a(N, N), b(N, N);
    a.setRotation((uint8_t)r);
    b.setRotation((uint8_t)r);
    a.fillRect(-5, 40, 30, 7, 0xF800);   // partly off screen, not square
    for (int y = 40; y < 47; y++)
      for (int x = -5; x < 25; x++) b.drawPixel(x, y, 0xF800);
    int diff = 0;
    for (int i = 0; i < N * N; i++) diff += a.pixels()[i] != b.pixels()[i];
    CHECK_EQ(diff, 0);
  }
}
