// Swipe-hub router (lib/app/nav.*): docs/design.md §2.
#include "framework.h"
#include "nav.h"

using nav::Result;
using nav::Router;
using nav::Screen;
using nav::Swipe;

TEST(nav_each_swipe_from_main_opens_its_screen) {
  struct { Swipe s; Screen want; } map[] = {
      {Swipe::Up, Screen::Status}, {Swipe::Left, Screen::Storage},
      {Swipe::Right, Screen::Chronicle}, {Swipe::Down, Screen::Settings}};
  for (auto &m : map) {
    Router r;
    CHECK(r.onSwipe(m.s, false, 0) == Result::Entered);
    CHECK(r.screen() == m.want);
    CHECK_EQ(r.page(), 0);
  }
}

TEST(nav_opposite_swipe_leaves) {
  Router r;
  r.onSwipe(Swipe::Up, false, 0);
  CHECK(r.onSwipe(Swipe::Down, false, 1) == Result::Left);
  CHECK(r.screen() == Screen::Main);
  r.onSwipe(Swipe::Left, false, 2);       // storage
  CHECK(r.onSwipe(Swipe::Right, false, 3) == Result::Left);
  CHECK(r.screen() == Screen::Main);
}

TEST(nav_pages_on_the_perpendicular_axis_and_clamps) {
  Router r;
  r.setPageCount(Screen::Status, 5);
  r.onSwipe(Swipe::Up, false, 0);
  CHECK(r.onSwipe(Swipe::Left, false, 1) == Result::Paged);
  CHECK_EQ(r.page(), 1);
  for (int i = 0; i < 10; i++) r.onSwipe(Swipe::Left, false, 2);
  CHECK_EQ(r.page(), 4);                  // clamped at the last page
  CHECK(r.onSwipe(Swipe::Right, false, 3) == Result::Paged);
  CHECK_EQ(r.page(), 3);
  CHECK(r.onSwipe(Swipe::Up, false, 4) == Result::None);   // entering swipe again: nothing
  CHECK(r.screen() == Screen::Status);

  Router c;
  c.setPageCount(Screen::Chronicle, 3);
  c.onSwipe(Swipe::Right, false, 0);
  CHECK(c.onSwipe(Swipe::Up, false, 1) == Result::Paged);   // chronicle pages up/down
  CHECK_EQ(c.page(), 1);
  CHECK(c.onSwipe(Swipe::Down, false, 2) == Result::Paged);
  CHECK_EQ(c.page(), 0);
}

TEST(nav_blocked_while_busy) {
  Router r;
  CHECK(r.onSwipe(Swipe::Up, true, 0) == Result::Blocked);
  CHECK(r.screen() == Screen::Main);
}

TEST(nav_play_and_starter_ignore_swipes) {
  Router r;
  r.go(Screen::Play, 0);
  CHECK(r.onSwipe(Swipe::Up, false, 1) == Result::None);
  CHECK(r.screen() == Screen::Play);
  CHECK(!r.tick(100000));                 // no auto-return out of the minigame
}

TEST(nav_auto_returns_after_30s_without_touch) {
  Router r;
  r.onSwipe(Swipe::Right, false, 1000);
  CHECK(!r.tick(1000 + 29999));
  r.touched(20000);
  CHECK(!r.tick(20000 + 29999));
  CHECK(r.tick(20000 + 30000));
  CHECK(r.screen() == Screen::Main);
}

TEST(nav_hints_marked_seen_when_screen_opened) {
  Router r;
  CHECK(r.hintPending(Swipe::Left));
  r.onSwipe(Swipe::Left, false, 0);
  CHECK(!r.hintPending(Swipe::Left));
  CHECK(r.hintPending(Swipe::Up));
  r.back(1);
  CHECK(r.screen() == Screen::Main);
  CHECK_EQ(r.hintsSeen(), 1 << 1);
}

TEST(nav_page_remembered_and_reset_on_entry) {
  Router r;
  r.setPageCount(Screen::Status, 5);
  r.onSwipe(Swipe::Up, false, 0);
  r.onSwipe(Swipe::Left, false, 0);
  r.back(0);
  r.onSwipe(Swipe::Up, false, 0);
  CHECK_EQ(r.page(), 0);                  // design: Status opens on "Next up"
}
