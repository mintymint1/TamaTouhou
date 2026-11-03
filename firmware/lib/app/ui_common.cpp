#include "ui_common.h"

#include <cstdio>
#include <cstring>

namespace ui {

using app::gfx;

tama::Pet *pet = nullptr;
nav::Router router;

Palette palette() {
  uint32_t e = pet->lastSeenEpoch();
  int hour = e ? (int)((e / 3600) % 24) : 13;
  bool night = pet->sleeping() || hour < 6 || hour >= 20;
  Palette p;
  p.night = night;
  p.ink = night ? tama::kPal_ink_night : tama::kPal_ink;
  p.bg = night ? tama::kPal_bg_night : tama::kPal_bg_day;
  p.outline = night ? tama::kPal_bg_night : 0x0000;
  p.dim = tama::kPal_track;
  return p;
}

// --------------------------------------------------------------------------- text

const char *T(const char *key, const char *fallback) { return tama::themeText(key, fallback); }

void fmt(char *out, size_t cap, const char *src, const FmtArgs &a) {
  size_t o = 0;
  for (const char *p = src; *p && o + 1 < cap;) {
    if (*p == '{') {
      const char *end = strchr(p, '}');
      if (end) {
        char key[24] = {0};
        size_t kl = (size_t)(end - p - 1);
        if (kl < sizeof(key)) memcpy(key, p + 1, kl);
        char num[16];
        const char *rep = nullptr;
        if (!strcmp(key, "name")) rep = a.name;
        else if (!strcmp(key, "other")) rep = a.other;
        else if (!strcmp(key, "tier")) rep = a.tier;
        else if (!strcmp(key, "n")) { snprintf(num, sizeof(num), "%ld", a.n); rep = num; }
        else if (!strcmp(key, "max")) { snprintf(num, sizeof(num), "%ld", a.max); rep = num; }
        else if (!strcmp(key, "egg")) rep = T("V_EGG", "EGG");
        else if (!strcmp(key, "collection")) rep = T("V_COLLECTION", "COLLECTION");
        else if (!strcmp(key, "storage")) rep = T("V_STORAGE", "STORAGE");
        else if (!strcmp(key, "keepsake")) rep = T("V_KEEPSAKE", "KEEPSAKE");
        else if (!strcmp(key, "evolve_verb")) rep = T("V_EVOLVE_VERB", "EVOLVE");
        if (rep) {
          for (const char *r = rep; *r && o + 1 < cap; r++) out[o++] = *r;
          p = end + 1;
          continue;
        }
      }
    }
    out[o++] = *p++;
  }
  out[o] = 0;
}

void fmt(char *out, size_t cap, const char *src, const char *name, long n) {
  FmtArgs a;
  a.name = name ? name : "";
  a.n = n;
  fmt(out, cap, src, a);
}

const char *speciesName(uint16_t idx) {
  return idx < tama::kTheme.speciesCount ? tama::kTheme.species[idx].name : "?";
}

const char *displayName() {
  if (pet->isEgg()) return T("V_EGG", "EGG");
  return pet->nick()[0] ? pet->nick() : pet->speciesDef()->name;
}

// --------------------------------------------------------------------------- drawing

int textW(const char *s, int size) { return (int)strlen(s) * 6 * size; }

void text(const char *s, int x, int y, int size, uint16_t col) {
  gfx->setTextSize(size);
  gfx->setTextColor(col);
  gfx->setCursor(x, y);
  gfx->print(s);
}

void textCentered(const char *s, int y, int size, uint16_t col) { text(s, CX - textW(s, size) / 2, y, size, col); }

void textOutlined(const char *s, int y, int size, uint16_t col, uint16_t outline) {
  int x = CX - textW(s, size) / 2;
  for (int dx = -1; dx <= 1; dx++)
    for (int dy = -1; dy <= 1; dy++)
      if (dx || dy) text(s, x + dx, y + dy, size, outline);
  text(s, x, y, size, col);
}

void drawItem(const Asset &a, int cx, int cy, int box) {
  if (!a.ok) return;
  const tama::SpriteSlot &s = a.pack.slot(tama::SLOT_IDLE);
  int dim = s.w > s.h ? s.w : s.h;
  int scale = dim ? box / dim : 1;
  if (scale < 1) scale = 1;
  drawSprite(a.pack, tama::SLOT_IDLE, 0, cx, cy + s.h * scale / 2, scale);
}

void drawBarH(int x, int y, int w, int h, uint32_t val, uint32_t max, uint16_t fill, uint16_t track) {
  gfx->fillRoundRect(x, y, w, h, h / 3 + 1, track);
  if (!max) return;
  uint32_t v = val > max ? max : val;
  int fw = (int)((w - 4) * v / max);
  if (fw > 0) gfx->fillRoundRect(x + 2, y + 2, fw, h - 4, h / 3, fill);
}

uint16_t needColour(uint8_t v) {
  return v >= 50 ? tama::kPal_bar_ok : v >= 25 ? tama::kPal_bar_warn : tama::kPal_bar_bad;
}

void fillBackground(uint16_t c) { gfx->fillScreen(c); }

// --------------------------------------------------------------------------- chrome

void drawTitle(const char *s, uint16_t col) {
  // the chord at y = 30 is ~250 px wide: shrink to size 1 if a long title won't fit
  int size = textW(s, 2) <= 240 ? 2 : 1;
  textCentered(s, kTitleY + (size == 1 ? 4 : 0), size, col);
}

void drawBackMark(uint16_t col) {   // a small chevron ("^") at the top of the circle
  for (int i = 0; i < 3; i++) {
    gfx->drawLine(CX - 12, 20 + i, CX, 10 + i, col);
    gfx->drawLine(CX, 10 + i, CX + 12, 20 + i, col);
  }
}

bool backMarkHit(int x, int y) { return y < 52 && x > CX - 60 && x < CX + 60; }

void drawPageDots(uint8_t n, uint8_t i, uint16_t on, uint16_t off) {
  if (n <= 1) return;
  int gap = 16, x0 = CX - (n - 1) * gap / 2;
  for (uint8_t k = 0; k < n; k++) gfx->fillCircle(x0 + k * gap, kDotsY, k == i ? 5 : 3, k == i ? on : off);
}

void drawDialog(const char *q, const char *yes, const char *no, uint16_t ink) {
  gfx->fillRoundRect(kDialogX, kDialogY, kDialogW, kDialogH, 16, 0xFFFF);
  gfx->drawRoundRect(kDialogX, kDialogY, kDialogW, kDialogH, 16, ink);
  // questions can be long ("Send KOGASA to THE GAP and bring out KITTEN?"): wrap at ~22 chars
  char line[48];
  size_t len = strlen(q);
  if (textW(q, 2) <= kDialogW - 24 || len >= sizeof(line)) {
    textCentered(q, kDialogY + 24, textW(q, 2) <= kDialogW - 24 ? 2 : 1, 0x2946);
  } else {
    size_t cut = len / 2;
    while (cut > 0 && q[cut] != ' ') cut--;
    memcpy(line, q, cut);
    line[cut] = 0;
    textCentered(line, kDialogY + 14, 2, 0x2946);
    textCentered(q + cut + 1, kDialogY + 36, 2, 0x2946);
  }
  gfx->fillRoundRect(kDialogX + 20, kDialogY + 64, kDialogW - 40, 40, 12, tama::kPal_bar_ok);
  textCentered(yes, kDialogY + 77, 2, 0xFFFF);
  gfx->fillRoundRect(kDialogX + 20, kDialogY + 114, kDialogW - 40, 40, 12, tama::kPal_track);
  textCentered(no, kDialogY + 127, 2, 0x2946);
  (void)ink;
}

uint8_t dialogHit(int x, int y) {
  if (hit(x, y, kDialogX + 20, kDialogY + 64, kDialogW - 40, 40)) return 1;
  if (hit(x, y, kDialogX + 20, kDialogY + 114, kDialogW - 40, 40)) return 2;
  return 0;
}

bool hit(int x, int y, int bx, int by, int bw, int bh) { return x >= bx && x <= bx + bw && y >= by && y <= by + bh; }

void fmtDuration(char *out, size_t cap, int32_t gameMinutes) {
  if (gameMinutes < 0) { snprintf(out, cap, "-"); return; }
  // ETAs are in game minutes; show real time at the current speed (the simulator can run faster)
  uint64_t sec = (uint64_t)gameMinutes * pet->tickMs() / 1000;
  if (sec < 60) snprintf(out, cap, "%us", (unsigned)sec);
  else if (sec < 3600) snprintf(out, cap, "%u min", (unsigned)(sec / 60));
  else if (sec < 86400) snprintf(out, cap, "%u h %u m", (unsigned)(sec / 3600), (unsigned)(sec % 3600 / 60));
  else snprintf(out, cap, "%u d %u h", (unsigned)(sec / 86400), (unsigned)(sec % 86400 / 3600));
}

// --------------------------------------------------------------------------- thumbnails

static uint8_t *thumbData = nullptr;
static size_t thumbLen = 0;
static bool thumbTried = false;

bool thumb(uint16_t species, tama::SpritePack &out) {
  if (!thumbTried) {
    thumbTried = true;
    if (!plat::readFile("/thumbs.bin", &thumbData, &thumbLen, 1024 * 1024)) thumbData = nullptr;
  }
  if (!thumbData || thumbLen < 6 || memcmp(thumbData, "TTH1", 4) != 0) return false;
  uint16_t count = (uint16_t)(thumbData[4] | thumbData[5] << 8);
  if (species >= count || 6 + 8u * count > thumbLen) return false;
  const uint8_t *e = thumbData + 6 + 8u * species;
  uint32_t off = e[0] | e[1] << 8 | e[2] << 16 | (uint32_t)e[3] << 24;
  uint32_t size = e[4] | e[5] << 8 | e[6] << 16 | (uint32_t)e[7] << 24;
  if (!size || off > thumbLen || size > thumbLen - off) return false;   // untrusted offsets
  return out.parse(thumbData + off, size);
}

void drawThumb(uint16_t species, int cx, int cy, int scale, bool variant, bool silhouette) {
  static tama::SpritePack sp;   // parsed on demand: keeps RAM low on the board
  if (!thumb(species, sp)) {
    textCentered("?", cy - 8, 2, tama::kPal_track);
    return;
  }
  const tama::SpriteSlot &s = sp.slot(tama::SLOT_IDLE);
  drawSprite(sp, tama::SLOT_IDLE, 0, cx, cy + s.h * scale / 2, scale, variant, false, 255,
             silhouette ? 0x18C4 : 0);
}

// --------------------------------------------------------------------------- banner

static char bannerText[64];
static uint32_t bannerUntil = 0;

void banner(const char *msg, uint32_t now, uint32_t ms) {
  strncpy(bannerText, msg, sizeof(bannerText) - 1);
  bannerText[sizeof(bannerText) - 1] = 0;
  bannerUntil = tama::deadlineIn(now, ms);
}

void drawBanner(uint32_t now) {
  if (!tama::timeLeft(bannerUntil, now)) return;
  int size = textW(bannerText, 2) <= 380 ? 2 : 1;
  int w = textW(bannerText, size) + 28;
  gfx->fillRoundRect(CX - w / 2, 98, w, 34, 14, 0x0000);
  gfx->drawRoundRect(CX - w / 2, 98, w, 34, 14, tama::kPal_bar_warn);
  textCentered(bannerText, size == 2 ? 108 : 112, size, 0xFFFF);
}

}  // namespace ui
