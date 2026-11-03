#include "nav.h"

namespace nav {

namespace {

// Which screen each swipe opens from Main.
Screen target(Swipe s) {
  switch (s) {
    case Swipe::Up: return Screen::Status;
    case Swipe::Left: return Screen::Storage;
    case Swipe::Right: return Screen::Chronicle;
    case Swipe::Down: return Screen::Settings;
  }
  return Screen::Main;
}

// The swipe that leaves a hub screen = the opposite of the one that entered it.
Swipe leaving(Screen s) {
  switch (s) {
    case Screen::Status: return Swipe::Down;
    case Screen::Storage: return Swipe::Right;
    case Screen::Chronicle: return Swipe::Left;
    case Screen::Settings: return Swipe::Up;
    default: return Swipe::Up;
  }
}

bool vertical(Swipe s) { return s == Swipe::Up || s == Swipe::Down; }
bool enteredVertically(Screen s) { return s == Screen::Status || s == Screen::Settings; }

}  // namespace

Result Router::onSwipe(Swipe s, bool busy, uint32_t now) {
  last_ = now;
  if (screen_ == Screen::Play || screen_ == Screen::Starter) return Result::None;
  if (busy) return Result::Blocked;
  if (screen_ == Screen::Main) {
    screen_ = target(s);
    page_[(int)screen_] = 0;
    hints_ |= (uint16_t)(1u << hintBit(s));
    return Result::Entered;
  }
  if (s == leaving(screen_)) {
    screen_ = Screen::Main;
    return Result::Left;
  }
  // perpendicular axis pages: Status/Settings page left/right, Storage/Chronicle up/down.
  if (vertical(s) == enteredVertically(screen_)) return Result::None;   // the entering swipe again
  uint8_t &p = page_[(int)screen_];
  uint8_t n = pages_[(int)screen_];
  bool next = s == Swipe::Left || s == Swipe::Up;   // left / up advance, like turning a page
  if (next && p + 1 < n) { p++; return Result::Paged; }
  if (!next && p > 0) { p--; return Result::Paged; }
  return Result::None;
}

Result Router::back(uint32_t now) {
  last_ = now;
  if (screen_ == Screen::Main || screen_ == Screen::Play || screen_ == Screen::Starter) return Result::None;
  screen_ = Screen::Main;
  return Result::Left;
}

void Router::go(Screen s, uint32_t now) {
  screen_ = s;
  last_ = now;
}

bool Router::tick(uint32_t now) {
  if (screen_ == Screen::Main || screen_ == Screen::Play || screen_ == Screen::Starter) return false;
  if ((int32_t)(now - last_) < (int32_t)kAutoReturnMs) return false;
  screen_ = Screen::Main;
  return true;
}

}  // namespace nav
