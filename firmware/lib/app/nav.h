#pragma once
// Screen router for the swipe hub (docs/design.md §2). Pure logic, no
// drawing, so it's host-tested (test/host/test_nav.cpp).
//
//                 ▲ swipe up: STATUS
//   ◄ swipe left: STORAGE ── MAIN ── CHRONICLE: swipe right ►
//                 ▼ swipe down: SETTINGS
//
// Enter with a swipe, leave with the opposite swipe (or the back mark). Inside a screen,
// page along the OTHER axis, so paging never collides with leaving. No navigation while the
// game is busy (evolution, ending, dialog, minigame). Any screen returns to Main after 30 s
// without a touch (AMOLED burn-in; the pet is the default view).
#include <cstdint>

namespace nav {

enum class Screen : uint8_t { Main = 0, Status, Storage, Chronicle, Settings, Play, Starter, Count };
enum class Swipe : uint8_t { Up, Down, Left, Right };

// What a swipe did, so the UI can play a sound / start a transition.
enum class Result : uint8_t { None, Entered, Left, Paged, Blocked };

class Router {
 public:
  static constexpr uint32_t kAutoReturnMs = 30000;

  Screen screen() const { return screen_; }
  uint8_t page() const { return page_[(int)screen_]; }
  uint8_t page(Screen s) const { return page_[(int)s]; }
  void setPageCount(Screen s, uint8_t n) { pages_[(int)s] = n ? n : 1; clampPage(s); }
  uint8_t pageCount(Screen s) const { return pages_[(int)s]; }

  // First-run hints: bit 0 up (Status), 1 left (Storage), 2 right (Chronicle), 3 down (Settings).
  uint16_t hintsSeen() const { return hints_; }
  void setHintsSeen(uint16_t h) { hints_ = h; }
  bool hintPending(Swipe s) const { return !(hints_ & (1u << hintBit(s))); }

  // `busy`: evolution / ending / dialog in progress: navigation is ignored.
  Result onSwipe(Swipe s, bool busy, uint32_t now);
  Result back(uint32_t now);              // the ⌃ back mark: same as the leaving swipe
  void go(Screen s, uint32_t now);        // explicit switch (Play, Starter, back to Main)
  void touched(uint32_t now) { last_ = now; }
  bool tick(uint32_t now);                // true when it auto-returned to Main

 private:
  static int hintBit(Swipe s) { return s == Swipe::Up ? 0 : s == Swipe::Left ? 1 : s == Swipe::Right ? 2 : 3; }
  void clampPage(Screen s) { if (page_[(int)s] >= pages_[(int)s]) page_[(int)s] = (uint8_t)(pages_[(int)s] - 1); }
  Screen screen_ = Screen::Main;
  uint8_t page_[(int)Screen::Count] = {0};
  uint8_t pages_[(int)Screen::Count] = {1, 5, 1, 1, 1, 1, 1};
  uint16_t hints_ = 0;
  uint32_t last_ = 0;
};

}  // namespace nav
