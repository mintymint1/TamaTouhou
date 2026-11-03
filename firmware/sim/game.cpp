#include "game.h"

#include "app_platform.h"
#include "gfx.h"
#include "theme_data.h"
#include "ui.h"

namespace pc {

Game::Game(const std::string &saveRoot)
    : epoch(plat::epoch),
      kv_(saveRoot + "/" + themeNamespace()),
      rng_(plat::random32()),
      pet_(tama::kTheme, kv_, rng_) {
  // A mouse drag is slower and less straight than a finger flick: relax the swipe thresholds
  // on the PC only (the board keeps 800 ms / 70 px from TamaPoke).
  gestures_.swipeMaxMs = 2500;
  gestures_.swipeCross = 140;
}

void Game::begin(uint32_t nowMs) {
  pet_.begin(nowMs);
  pet_.syncClock(epoch());   // the pet aged while the app was closed (capped at 14 days)
  ui::begin(pet_);
  lastSample_ = lastRender_ = lastSave_ = nowMs;
}

void Game::pointer(bool down, int x, int y) {
  glassToScreen(plat::rotation(), x, y);   // the canvas may be turned (Settings)
  down_ = down;
  mx_ = x;
  my_ = y;
}

void Game::swipe(tama::Gesture::Kind kind, uint32_t nowMs) {
  tama::Gesture g;
  g.kind = kind;
  g.x = CX;
  g.y = CY;
  ui::onGesture(g, nowMs);
}

void Game::gesture(const tama::Gesture &g, uint32_t nowMs) { ui::onGesture(g, nowMs); }

uint32_t Game::step(uint32_t now, bool visible) {
  uint32_t e = epoch();
  if (pet_.resumeIfSuspended(now, e)) plat::log("resumed after the computer slept\n");
  pet_.update(now);
  ui::onEvents(pet_.takeEvents());
  if (now - lastSample_ >= 20) {   // the board samples touch at 50 Hz
    lastSample_ = now;
    gestures_.longPressMs = ui::longPressMs();
    ui::onGesture(gestures_.feed(down_, (int16_t)mx_, (int16_t)my_, now), now);
  }
  pet_.noteEpoch(e);
  if (pet_.savePending() && now - lastSave_ > 10000) {
    pet_.flushSave();
    lastSave_ = now;
  }
  uint32_t frame = ui::frameIntervalMs();
  if (visible && now - lastRender_ >= frame) renderNow(now);

  // How long the front end may sleep: a held button needs the 50 Hz sampling (long press);
  // a visible screen needs its next frame; hidden, a second is plenty (ticks are minutes).
  if (down_) return 20;
  if (!visible) return 1000;
  uint32_t since = now - lastRender_;
  return since >= frame ? 1 : frame - since;
}

void Game::renderNow(uint32_t now) {
  lastRender_ = now;
  ui::render(now);
}

void Game::shutdown() { pet_.save(); }

}  // namespace pc
