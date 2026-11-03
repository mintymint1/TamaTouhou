#pragma once
// Turns raw touch samples into gestures (TamaPoke's thresholds, verified on the 466 px round
// screen). Hardware-free so it can be host-tested. Gestures resolve when the finger LIFTS,
// so a tap is never mistaken for the start of a swipe; long-press fires while still held.
#include <cstdint>
#include <cstdlib>

namespace tama {

struct Gesture {
  enum Kind : uint8_t { None, Tap, SwipeLeft, SwipeRight, SwipeUp, SwipeDown, LongPress } kind = None;
  int16_t x = 0, y = 0;   // where the gesture started
};

class GestureRecognizer {
 public:
  uint32_t longPressMs = 3000;   // long press on the pet = release dialog
  int16_t swipeMin = 80, swipeCross = 70, tapSlop = 40, holdSlop = 30;
  uint32_t swipeMaxMs = 800, tapMaxMs = 1500;

  // `swallow`: the first touch on a dimmed screen only wakes it and must not act.
  Gesture feed(bool pressed, int16_t x, int16_t y, uint32_t now, bool swallow = false) {
    Gesture g;
    if (pressed && !down_) {            // finger down
      down_ = true;
      x0_ = xl_ = x; y0_ = yl_ = y;
      t0_ = now;
      held_ = false;
      swallow_ = swallow;
    } else if (pressed) {               // still down
      xl_ = x; yl_ = y;
      if (!held_ && !swallow_ && now - t0_ >= longPressMs &&
          std::abs(xl_ - x0_) < holdSlop && std::abs(yl_ - y0_) < holdSlop) {
        held_ = true;
        g.kind = Gesture::LongPress; g.x = x0_; g.y = y0_;
      }
    } else if (down_) {                 // lift: resolve
      down_ = false;
      if (held_ || swallow_) return g;
      int dx = xl_ - x0_, dy = yl_ - y0_;
      uint32_t dt = now - t0_;
      g.x = x0_; g.y = y0_;
      if (std::abs(dx) > swipeMin && std::abs(dy) < swipeCross && dt < swipeMaxMs)
        g.kind = dx > 0 ? Gesture::SwipeRight : Gesture::SwipeLeft;
      else if (std::abs(dy) > swipeMin && std::abs(dx) < swipeCross && dt < swipeMaxMs)
        g.kind = dy > 0 ? Gesture::SwipeDown : Gesture::SwipeUp;
      else if (dt < tapMaxMs && std::abs(dx) < tapSlop && std::abs(dy) < tapSlop)
        g.kind = Gesture::Tap;
    }
    return g;
  }
  bool isDown() const { return down_; }

 private:
  bool down_ = false, held_ = false, swallow_ = false;
  int16_t x0_ = 0, y0_ = 0, xl_ = 0, yl_ = 0;
  uint32_t t0_ = 0;
};

}  // namespace tama
