#pragma once
// Shared pieces for every screen: layout constants, theme text, drawing helpers, the
// round-screen chrome (title on the top arc, back mark, page dots), thumbnails, banners.
#include <cstddef>
#include <cstdint>

#include "app_platform.h"
#include "assets.h"
#include "gfx.h"
#include "nav.h"
#include "pet.h"
#include "theme_data.h"

namespace ui {

// --- layout (466x466 round screen; content stays inside r = 233)
constexpr int kGround = 304;     // pet's feet on the main screen
constexpr int kPanelY = 312;     // bottom panel (bars + arc buttons)
constexpr int kDialogX = 83, kDialogY = 150, kDialogW = 300, kDialogH = 170;
constexpr int kTitleY = 30;      // screen title on the top arc
constexpr int kDotsY = 440;      // page dots on the bottom arc

extern tama::Pet *pet;
extern nav::Router router;

// --- colours for the current light (night = real night or the pet asleep)
struct Palette {
  bool night;
  uint16_t ink, bg, outline, dim;
};
Palette palette();

// --- theme text
const char *T(const char *key, const char *fallback);
// Fills {egg} {collection} {storage} {keepsake} {evolve_verb} (theme vocabulary) and the
// per-call {name} {other} {tier} {n} {max} placeholders.
struct FmtArgs {
  const char *name = "", *other = "", *tier = "";
  long n = 0, max = 0;
};
void fmt(char *out, size_t cap, const char *src, const FmtArgs &a);
void fmt(char *out, size_t cap, const char *src, const char *name = "", long n = 0);
const char *speciesName(uint16_t idx);
const char *displayName();     // active pet: nickname or species name

// --- drawing
int textW(const char *s, int size);
void text(const char *s, int x, int y, int size, uint16_t col);
void textCentered(const char *s, int y, int size, uint16_t col);
void textOutlined(const char *s, int y, int size, uint16_t col, uint16_t outline);
void drawItem(const Asset &a, int cx, int cy, int box);
void drawBarH(int x, int y, int w, int h, uint32_t val, uint32_t max, uint16_t fill, uint16_t track);
uint16_t needColour(uint8_t v);
void fillBackground(uint16_t c);   // whole screen, for the non-scene screens

// --- round-screen chrome
void drawTitle(const char *s, uint16_t col);
void drawBackMark(uint16_t col);
bool backMarkHit(int x, int y);
void drawPageDots(uint8_t n, uint8_t i, uint16_t on, uint16_t off);

// --- yes/no dialog (shared look)
void drawDialog(const char *q, const char *yes, const char *no, uint16_t ink);
uint8_t dialogHit(int x, int y);   // 0 = outside, 1 = yes, 2 = no
bool hit(int x, int y, int bx, int by, int bw, int bh);

// --- time: game minutes -> "42 min" / "2 h 40 m" / "3 d 4 h" in REAL time at the current speed
void fmtDuration(char *out, size_t cap, int32_t gameMinutes);

// --- thumbnails (firmware/data/thumbs.bin): idle frame 0 per species, <= 36 px
bool thumb(uint16_t species, tama::SpritePack &out);
// Draws a thumbnail centred on (cx, cy). silhouette: unseen entries in the Chronicle.
void drawThumb(uint16_t species, int cx, int cy, int scale, bool variant, bool silhouette);

// --- transient banner over any screen ("CHEN was kept in THE GAP")
void banner(const char *msg, uint32_t now, uint32_t ms = 2600);
void drawBanner(uint32_t now);

}  // namespace ui
