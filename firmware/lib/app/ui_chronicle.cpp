// Chronicle (swipe right): every roster entry. Never seen = dark silhouette; collected =
// sprite; ★ = rare colour found; small box = it's in storage. Pages scroll up/down.
// Tap an entry for its detail: animated sprite, tier, habitat, evolution line.
#include <cstdio>
#include <cstring>

#include "ui_common.h"
#include "ui_screens.h"

namespace ui {

using app::gfx;

constexpr int kCols = 4, kRows = 4, kCell = 76;
constexpr int kGridX = CX - kCols * kCell / 2, kGridY = 70;

static int16_t detail = -1;          // roster index shown in detail, -1 = grid
static bool detailVariant = false;
static Asset detailArt;

uint8_t chroniclePages() { return (uint8_t)((tama::kTheme.speciesCount + kCols * kRows - 1) / (kCols * kRows)); }

void chronicleEnter() {
  detail = -1;
  detailArt.unload();
}

static bool seen(uint16_t i) { return pet->isCollected(i) || pet->isVariantCollected(i); }
static bool stored(uint16_t i) { return pet->findStored(i, false) >= 0 || pet->findStored(i, true) >= 0; }

// Pretty habitat id: "human_village" -> "HUMAN VILLAGE"
static void habitatName(uint8_t h, char *out, size_t cap) {
  snprintf(out, cap, "%s", tama::themeHabitat(h));
  for (char *c = out; *c; c++) *c = *c == '_' ? ' ' : (char)(*c >= 'a' && *c <= 'z' ? *c - 32 : *c);
}

static void renderGrid(const Palette &p) {
  uint16_t n = tama::kTheme.speciesCount;
  char title[48];
  fmt(title, sizeof(title), "{collection} {n}", "", pet->collectedCount());
  snprintf(title + strlen(title), sizeof(title) - strlen(title), "/%u", n);
  drawTitle(title, p.ink);
  uint16_t first = (uint16_t)(router.page() * kCols * kRows);
  for (int k = 0; k < kCols * kRows; k++) {
    uint16_t i = (uint16_t)(first + k);
    if (i >= n) break;
    int cx = kGridX + (k % kCols) * kCell + kCell / 2, cy = kGridY + (k / kCols) * kCell + kCell / 2;
    gfx->drawRoundRect(cx - kCell / 2 + 3, cy - kCell / 2 + 3, kCell - 6, kCell - 6, 10, p.dim);
    bool s = seen(i);
    // only the rare colour found? show that one
    bool variantOnly = !pet->isCollected(i) && pet->isVariantCollected(i);
    drawThumb(i, cx, cy, 2, variantOnly, !s);
    if (pet->isVariantCollected(i)) text("*", cx + 22, cy - 34, 2, tama::kPal_bar_warn);
    if (stored(i)) gfx->fillRoundRect(cx + 20, cy + 22, 10, 9, 2, tama::kPal_accent_default);
  }
  drawPageDots(chroniclePages(), router.page(), p.ink, p.dim);
}

// Detail-view buttons: one is centred on the screen, two sit symmetrically around the centre.
constexpr int kBtnY = 314, kBtnW = 130, kBtnH = 36, kBtnGap = 20;
struct Buttons {
  bool toggle = false, gap = false;
  int toggleX = 0, gapX = 0;
};
static Buttons buttons() {
  Buttons b;
  b.toggle = pet->isCollected(detail) && pet->isVariantCollected(detail);
  b.gap = stored(detail);
  if (b.toggle && b.gap) {
    b.toggleX = CX - kBtnGap / 2 - kBtnW;
    b.gapX = CX + kBtnGap / 2;
  } else {
    b.toggleX = b.gapX = CX - kBtnW / 2;
  }
  return b;
}

static void renderDetail(uint32_t now, const Palette &p) {
  const tama::SpeciesDef &d = tama::kTheme.species[detail];
  char line[64];
  drawTitle(d.name, d.accent565);
  if (!detailArt.ok) {
    snprintf(line, sizeof(line), "/sprites/%s.spr", d.id);
    detailArt.load(line);
  }
  if (detailArt.ok) {
    int scale = spriteScale(detailArt.pack, 130);
    drawSprite(detailArt.pack, tama::SLOT_IDLE, detailArt.pack.frameAt(tama::SLOT_IDLE, now), CX, 210, scale, detailVariant);
  }
  const char *tier = d.tier == tama::kTierEvo ? T("S_CH_EVOLVED", "evolved form") : tama::themeTierHint(d.tier);
  char hab[32];
  habitatName(d.habitat, hab, sizeof(hab));
  snprintf(line, sizeof(line), "%s \xfa %s", tier[0] ? tier : T("S_CH_COMMON", "common"), hab);
  textCentered(line, 222, 1, p.ink);
  // evolution line: what it comes from and what it becomes
  int16_t from = -1;
  for (uint16_t i = 0; i < tama::kTheme.speciesCount && from < 0; i++)
    for (uint8_t e = 0; e < tama::kTheme.species[i].evoCount; e++)
      if (tama::kTheme.species[i].evos[e].to == (uint16_t)detail) from = (int16_t)i;
  int y = 268;
  if (from >= 0) drawThumb((uint16_t)from, 120, y, 1, false, !seen((uint16_t)from));
  if (from >= 0) text("\x1a", 146, y - 8, 2, p.ink);
  drawThumb((uint16_t)detail, CX, y, 1, detailVariant, false);
  for (uint8_t e = 0; e < d.evoCount && e < 2; e++) {
    text("\x1a", 270, y - 8 + e * 30 - (d.evoCount > 1 ? 15 : 0), 2, p.ink);
    drawThumb(d.evos[e].to, 330, y + e * 30 - (d.evoCount > 1 ? 15 : 0), 1, false, !seen(d.evos[e].to));
  }
  if (pet->findStored((uint16_t)detail, detailVariant) >= 0)   // discoverability for "hold to switch"
    textCentered(T("S_CH_HOLD_HINT", "Hold to bring out"), 296, 1, p.ink);
  // buttons: colour toggle (both colours found) and the "in storage" link, centred as a group
  Buttons b = buttons();
  if (b.toggle) {
    gfx->fillRoundRect(b.toggleX, kBtnY, kBtnW, kBtnH, 12, p.dim);
    const char *label = detailVariant ? T("S_CH_NORMAL", "NORMAL") : T("V_VARIANT", "RARE");
    text(label, b.toggleX + kBtnW / 2 - textW(label, 1) / 2, kBtnY + 14, 1, p.ink);
  }
  if (b.gap) {
    gfx->fillRoundRect(b.gapX, kBtnY, kBtnW, kBtnH, 12, tama::kPal_accent_default);
    fmt(line, sizeof(line), T("S_CH_IN_STORAGE", "In {storage}"));
    text(line, b.gapX + kBtnW / 2 - textW(line, 1) / 2, kBtnY + 14, 1, 0xFFFF);
  }
}

void chronicleRender(uint32_t now) {
  Palette p = palette();
  fillBackground(p.bg);
  drawBackMark(p.ink);
  if (detail >= 0) renderDetail(now, p);
  else renderGrid(p);
}

void chronicleTap(int x, int y, uint32_t now) {
  (void)now;
  if (detail >= 0) {
    Buttons b = buttons();
    if (b.toggle && hit(x, y, b.toggleX, kBtnY, kBtnW, kBtnH)) {
      detailVariant = !detailVariant;
    } else if (b.gap && hit(x, y, b.gapX, kBtnY, kBtnW, kBtnH)) {
      bool v = pet->findStored(detail, detailVariant) >= 0 ? detailVariant : !detailVariant;
      uint16_t sp = (uint16_t)detail;
      chronicleEnter();
      router.go(nav::Screen::Storage, now);
      storageOpen(sp, v);
    }
    return;
  }
  int col = (x - kGridX) / kCell, row = (y - kGridY) / kCell;
  if (x < kGridX || y < kGridY || col >= kCols || row >= kRows) return;   // check before dividing (TamaPoke 1.10)
  uint16_t i = (uint16_t)(router.page() * kCols * kRows + row * kCols + col);
  if (i >= tama::kTheme.speciesCount || !seen(i)) return;
  detail = (int16_t)i;
  detailVariant = !pet->isCollected(i);
  detailArt.unload();
}

// Hold a creature (grid cell or detail) to switch it in from storage, if a copy is stored.
void chronicleLongPress(int x, int y, uint32_t now) {
  int16_t sp = -1;
  bool variant = false;
  if (detail >= 0) {
    sp = detail;
    variant = detailVariant;   // the toggled colour
  } else {
    if (x < kGridX || y < kGridY) return;   // check before dividing (TamaPoke 1.10)
    int col = (x - kGridX) / kCell, row = (y - kGridY) / kCell;
    if (col >= kCols || row >= kRows) return;
    int i = router.page() * kCols * kRows + row * kCols + col;
    if (i >= tama::kTheme.speciesCount) return;
    sp = (int16_t)i;
    // the stored colour: normal unless only the rare colour is stored
    variant = pet->findStored((uint16_t)sp, false) < 0 && pet->findStored((uint16_t)sp, true) >= 0;
  }
  if (!seen((uint16_t)sp)) return;   // silhouettes: nothing to bring out
  char msg[64];
  if (pet->isEgg()) {
    fmt(msg, sizeof(msg), T("S_HATCH_FIRST", "Hatch the {egg} first"));
    banner(msg, now, 1800);
    return;
  }
  if (pet->species() == (uint16_t)sp && pet->variant() == variant) {
    fmt(msg, sizeof(msg), T("S_SG_ACTIVE_HINT", "{name} is with you"), displayName());
    banner(msg, now, 1500);
    return;
  }
  if (pet->findStored((uint16_t)sp, variant) < 0) {
    fmt(msg, sizeof(msg), T("S_CH_NOT_STORED", "Not in {storage}"));
    banner(msg, now, 1500);
    return;
  }
  if (storageAskSwitch((uint16_t)sp, variant, now)) {
    chronicleEnter();
    router.go(nav::Screen::Storage, now);
    plat::sfx(plat::SFX_TAP);
  }
}

bool chronicleSwipe(nav::Swipe s) {
  if (detail < 0) return false;
  if (s == nav::Swipe::Left) chronicleEnter();   // the leaving swipe closes the detail first
  return true;                                   // other swipes do nothing inside the detail
}

bool chronicleBack() {
  if (detail < 0) return false;
  chronicleEnter();
  return true;
}

}  // namespace ui
