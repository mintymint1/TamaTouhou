// Settings (swipe down), plus the main-screen glance elements: the bezel progress arc for
// the nearest milestone and the first-run edge hints.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui_common.h"
#include "ui_screens.h"

namespace ui {

using app::gfx;

// uiFlags bits (player-wide, persisted by the engine): 0-3 hints seen, 4 bezel arc OFF, 5 sound off
constexpr uint16_t kFlagArcOff = 1u << 4, kFlagSoundOff = 1u << 5;

// --------------------------------------------------------------------------- settings

constexpr int kRowY0 = 86, kRowH = 58;
static const char *kBrightness[4] = {"AUTO", "LOW", "MID", "HIGH"};

// Battery at the top of Settings, under the title: a gauge, the percentage, and the power
// state (charging / full / USB power). Hidden when there's neither a battery nor USB to
// report (a desktop PC). Green from 50 %, amber from 20 %, red below.
static void drawBattery(const Palette &p) {
  plat::Power pw = plat::power();
  if (pw.percent < 0 && !pw.external) return;
  char pct[8] = "";
  if (pw.percent >= 0) snprintf(pct, sizeof(pct), "%d%%", pw.percent);
  const char *state = "";
  if (pw.charging) state = T("S_BAT_CHARGING", "charging");
  else if (pw.external && pw.percent >= 95) state = T("S_BAT_FULL", "full");
  else if (pw.external && pw.percent >= 0) state = T("S_BAT_PLUGGED", "plugged in");
  else if (pw.external) state = T("S_BAT_USB", "USB power");

  constexpr int kIconW = 30, kIconH = 16, kNub = 3, kGap = 8, kY = 54;
  int tw = pw.percent >= 0 ? textW(pct, 2) : 0, sw = state[0] ? textW(state, 2) : 0;
  int total = (pw.percent >= 0 ? kIconW + kNub + kGap + tw : 0) + (sw ? (tw ? kGap : 0) + sw : 0);
  int x = CX - total / 2;
  if (pw.percent >= 0) {
    uint16_t level = pw.percent >= 50 ? tama::kPal_bar_ok : pw.percent >= 20 ? tama::kPal_bar_warn : tama::kPal_bar_bad;
    gfx->drawRoundRect(x, kY, kIconW, kIconH, 3, p.ink);
    gfx->fillRect(x + kIconW, kY + 5, kNub, kIconH - 10, p.ink);
    int fw = (kIconW - 4) * pw.percent / 100;
    if (fw > 0) gfx->fillRect(x + 2, kY + 2, fw, kIconH - 4, level);
    if (pw.charging) {   // lightning bolt over the gauge: dark edge, then bright yellow
      int cx = x + kIconW / 2, cy = kY + kIconH / 2;
      const int pts[4][2] = {{cx + 3, cy - 6}, {cx - 2, cy + 1}, {cx + 2, cy + 1}, {cx - 3, cy + 6}};
      for (int pass = 0; pass < 2; pass++)
        for (int s = 0; s < 3; s++)
          for (int t = -1; t <= (pass ? 0 : 1); t++)
            gfx->drawLine(pts[s][0] + t, pts[s][1], pts[s + 1][0] + t, pts[s + 1][1],
                          pass ? tama::kPal_bar_warn : p.outline);
    }
    text(pct, x + kIconW + kNub + kGap, kY + 1, 2, p.ink);
    x += kIconW + kNub + kGap + tw + kGap;
  }
  if (sw) text(state, x, kY + 1, 2, pw.charging ? tama::kPal_bar_ok : p.ink);   // readable: size 2
}

// Settings rows: clock, sound, brightness, progress ring, rotate screen.
static int rowCount() { return 5; }

static void row(int i, const char *label, const char *value, const Palette &p) {
  int y = kRowY0 + i * kRowH;
  gfx->drawRoundRect(78, y, 310, kRowH - 10, 12, p.dim);
  text(label, 94, y + 16, 2, p.ink);
  text(value, 372 - textW(value, 2), y + 16, 2, tama::kPal_accent_default);
}

void settingsRender(uint32_t now) {
  (void)now;
  Palette p = palette();
  fillBackground(p.bg);
  drawBackMark(p.ink);
  drawTitle(T("S_SET_TITLE", "SETTINGS"), p.ink);
  drawBattery(p);
  // clock: tap the left/right half of the row to go back/forward an hour; the minutes row likewise
  uint32_t e = plat::epoch();
  char clock[16];
  snprintf(clock, sizeof(clock), "%02u:%02u", (unsigned)(e / 3600 % 24), (unsigned)(e / 60 % 60));
  row(0, T("S_SET_CLOCK", "Clock"), clock, p);
  text("-", 196, kRowY0 + 16, 2, p.dim);
  text("+", 270, kRowY0 + 16, 2, p.dim);
  uint16_t f = pet->uiFlags();
  row(1, T("S_SET_SOUND", "Sound"), (f & kFlagSoundOff) ? T("S_OFF", "OFF") : T("S_ON", "ON"), p);
  uint8_t b = pet->brightness() < 4 ? pet->brightness() : 0;
  row(2, T("S_SET_BRIGHTNESS", "Brightness"), kBrightness[b], p);
  row(3, T("S_SET_ARC", "Progress ring"), (f & kFlagArcOff) ? T("S_OFF", "OFF") : T("S_ON", "ON"), p);
  // rotate: each tap turns the picture a quarter clockwise: 0, 90, 180, 270
  char deg[8];
  snprintf(deg, sizeof(deg), "%d", plat::rotation() * 90);
  row(4, T("S_SET_ROTATE", "Rotate screen"), deg, p);
  // degree sign drawn, not printed: the 5x7 font's code page differs between board and PC
  gfx->drawCircle(376, kRowY0 + 4 * kRowH + 17, 2, tama::kPal_accent_default);
  char ver[48];
  snprintf(ver, sizeof(ver), "%s v%u", tama::kTheme.id, tama::kTheme.version);
  textCentered(ver, kRowY0 + rowCount() * kRowH + 10, 1, p.dim);
}

void settingsTap(int x, int y, uint32_t now) {
  (void)now;
  int i = (y - kRowY0) / kRowH;
  if (y < kRowY0 || i >= rowCount() || x < 78 || x > 388) return;
  uint16_t f = pet->uiFlags();
  plat::sfx(plat::SFX_TAP);
  switch (i) {
    case 0: {   // clock: left of centre = -1 h, right = +1 h; outer thirds = minutes
      int32_t step = x < 160 ? -60 : x < CX ? -3600 : x < 306 ? 3600 : 60;
      uint32_t e = (uint32_t)((int64_t)plat::epoch() + step);
      plat::setEpoch(e);
      pet->setClock(e);
      break;
    }
    case 1:
      f ^= kFlagSoundOff;
      pet->setUiFlags(f);
      plat::setSoundEnabled(!(f & kFlagSoundOff));
      break;
    case 2: {
      uint8_t b = (uint8_t)((pet->brightness() + 1) % 4);
      pet->setBrightness(b);
      plat::setBrightnessLevel(b);
      break;
    }
    case 3:
      pet->setUiFlags(f ^ kFlagArcOff);
      break;
    case 4:
      plat::setRotation((uint8_t)((plat::rotation() + 1) & 3));
      break;
  }
}

// --------------------------------------------------------------------------- main-screen glance

void drawBezelArc() {
  if (pet->uiFlags() & kFlagArcOff) return;
  tama::Milestone m[tama::kMaxMilestones];
  uint8_t n = pet->milestones(m, tama::kMaxMilestones);
  if (!n || !m[0].target) return;
  float frac = (float)(m[0].current > m[0].target ? m[0].target : m[0].current) / (float)m[0].target;
  // thin ring just inside the round edge, starting at 12 o'clock, clockwise
  // 4 px wide ring, plotted densely enough (0.25 degree) to be solid at r = 230
  const float r0 = 226.0f;
  for (int step = 0; step < 1440; step++) {
    float a = (step / 4.0f - 90.0f) * 3.14159265f / 180.0f;
    uint16_t col = step < frac * 1440 ? tama::kPal_accent_default : 0x2104;
    gfx->fillRect(CX + (int)lroundf(cosf(a) * r0) - 2, CY + (int)lroundf(sinf(a) * r0) - 2, 4, 4, col);
  }
}

void drawEdgeHints(uint16_t col) {
  // Labelled arrows at the four edges until each screen has been opened once. The arrow shows
  // the finger's direction: swipe up opens Status, so its hint sits low with an up arrow.
  struct Hint { nav::Swipe s; const char *key, *fallback, *arrow; int x, y; };
  static const Hint hints[4] = {
      {nav::Swipe::Up, "S_HINT_UP", "Status", "\x18", CX, 286},
      {nav::Swipe::Down, "S_HINT_DOWN", "Settings", "\x19", CX, 92},
      {nav::Swipe::Left, "S_HINT_LEFT", "Storage", "\x1b", 420, 196},
      {nav::Swipe::Right, "S_HINT_RIGHT", "Chronicle", "\x1a", 46, 196},
  };
  for (const Hint &h : hints) {
    if (!router.hintPending(h.s)) continue;
    const char *label = T(h.key, h.fallback);
    char line[32];
    if (h.s == nav::Swipe::Left) snprintf(line, sizeof(line), "%s%s", h.arrow, label);
    else if (h.s == nav::Swipe::Right) snprintf(line, sizeof(line), "%s%s", label, h.arrow);
    else snprintf(line, sizeof(line), "%s %s", h.arrow, label);
    int w = textW(line, 1);
    int x = h.s == nav::Swipe::Left ? h.x - w : h.s == nav::Swipe::Right ? h.x : h.x - w / 2;
    gfx->fillRoundRect(x - 5, h.y - 4, w + 10, 16, 6, 0x0000);
    text(line, x, h.y, 1, col);
  }
}

}  // namespace ui
