// Touch gesture recogniser (lib/engine/gesture.h): TamaPoke's thresholds.
#include "framework.h"
#include "gesture.h"

using namespace tama;

namespace {
// Feed a straight-line drag from (x0,y0) to (x1,y1) over `ms`, sampled every 20 ms, then lift.
Gesture drag(GestureRecognizer &g, int x0, int y0, int x1, int y1, uint32_t ms, uint32_t t = 1000,
             bool swallow = false) {
  Gesture out;
  int steps = (int)(ms / 20);
  for (int i = 0; i <= steps; i++) {
    int x = x0 + (x1 - x0) * i / (steps ? steps : 1), y = y0 + (y1 - y0) * i / (steps ? steps : 1);
    Gesture r = g.feed(true, (int16_t)x, (int16_t)y, t + i * 20, swallow && i == 0);
    if (r.kind != Gesture::None) out = r;
  }
  Gesture r = g.feed(false, 0, 0, t + ms + 20);
  return r.kind != Gesture::None ? r : out;
}
}  // namespace

TEST(gesture_tap) {
  GestureRecognizer g;
  Gesture r = drag(g, 200, 200, 205, 203, 120);
  CHECK(r.kind == Gesture::Tap);
  CHECK_EQ(r.x, 200);
}

TEST(gesture_swipes_in_four_directions) {
  GestureRecognizer g;
  CHECK(drag(g, 100, 233, 300, 240, 300).kind == Gesture::SwipeRight);
  CHECK(drag(g, 300, 233, 100, 240, 300).kind == Gesture::SwipeLeft);
  CHECK(drag(g, 233, 300, 240, 100, 300).kind == Gesture::SwipeUp);
  CHECK(drag(g, 233, 100, 240, 300, 300).kind == Gesture::SwipeDown);
}

TEST(gesture_slow_or_diagonal_drag_is_nothing) {
  GestureRecognizer g;
  CHECK(drag(g, 100, 233, 300, 240, 1200).kind == Gesture::None);   // too slow for a swipe
  CHECK(drag(g, 100, 100, 300, 300, 300).kind == Gesture::None);    // diagonal
}

TEST(gesture_long_press_fires_while_held_and_suppresses_tap) {
  GestureRecognizer g;
  Gesture held;
  for (uint32_t t = 0; t <= 3200; t += 20) {
    Gesture r = g.feed(true, 233, 200, 1000 + t);
    if (r.kind != Gesture::None) held = r;
  }
  CHECK(held.kind == Gesture::LongPress);
  CHECK(g.feed(false, 0, 0, 4300).kind == Gesture::None);   // no tap on release
}

TEST(gesture_wake_touch_is_swallowed) {
  GestureRecognizer g;
  CHECK(drag(g, 200, 200, 202, 201, 100, 1000, true).kind == Gesture::None);
  CHECK(drag(g, 200, 200, 202, 201, 100, 2000, false).kind == Gesture::Tap);
}
