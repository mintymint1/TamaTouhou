// Storage (swipe left): creatures kept in stasis (Touhou wording: "THE GAP"). Grid of 3x3
// pages (up/down), a detail page with SWITCH, "store & new egg", and release by long-press.
// The rules live in the engine (Pet::switchWith / storeActiveAndNewEgg / releaseStored).
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui_common.h"
#include "ui_screens.h"

namespace ui {

using app::gfx;
using tama::StoreError;

constexpr int kCols = 3, kRows = 3, kCell = 80;
constexpr int kGridX = CX - kCols * kCell / 2, kGridY = 66;
constexpr int kStoreBtnY = 316, kStoreBtnW = 280, kStoreBtnH = 42;
constexpr uint32_t kEffectMs = 1100;

enum class Dialog : uint8_t { None, Switch, ReplaceSwitch, Store, ReplaceStore, Release };

static int16_t detail = -1;      // slot shown in detail, -1 = grid
static Dialog dialog = Dialog::None;
static int16_t dialogSlot = -1;
static uint32_t effectUntil = 0;
static Asset detailArt;

// Grid cells: the active pet first ("with you", not selectable), then the stored slots, so
// what's on screen matches the ownedCount() in the title.
static int activeCells() { return pet->isEgg() ? 0 : 1; }
static int cellCount() { return pet->storageCount() + activeCells(); }

uint8_t storagePages() {
  int n = cellCount();
  return (uint8_t)(n ? (n + kCols * kRows - 1) / (kCols * kRows) : 1);
}

// Level as a small badge in the cell's top-left corner (a caption under the sprite collided
// with the cell border).
static void levelBadge(int cx, int cy, uint16_t lv, uint16_t ink) {
  char t[8];
  snprintf(t, sizeof(t), "%u", lv);
  int w = textW(t, 1) + 6;
  gfx->fillRoundRect(cx - kCell / 2 + 6, cy - kCell / 2 + 6, w, 11, 3, 0x0000);
  text(t, cx - kCell / 2 + 9, cy - kCell / 2 + 8, 1, ink);
}

void storageEnter() {
  detail = -1;
  dialog = Dialog::None;
  detailArt.unload();
}

bool storageBusy(uint32_t now) { return dialog != Dialog::None || tama::timeLeft(effectUntil, now) > 0; }

void storageOpen(uint16_t species, bool variant) {
  storageEnter();
  int16_t s = pet->findStored(species, variant);
  if (s >= 0) detail = s;
}

static const char *specimenName(const tama::Specimen &s) { return s.nick[0] ? s.nick : speciesName(s.species); }

// Why "store + new egg" isn't available yet (engine gate, docs/design.md
// §10): it has to evolve first, or (nothing left to evolve into) spend more time with you.
static void storeGateText(StoreError e, char *out, size_t cap) {
  if (e == StoreError::NotAwakened) {
    fmt(out, cap, T("S_STORE_NOT_AWAKENED", "{evolve_verb} {name} first"), displayName());
  } else {
    char dur[24];
    fmtDuration(dur, sizeof(dur), (int32_t)pet->storeReadyInMinutes());
    fmt(out, cap, T("S_STORE_CARE_LEFT", "Ready in {name}"), dur);
  }
}

static void errorBanner(StoreError e, uint32_t now) {
  char msg[64];
  switch (e) {
    case StoreError::Egg: fmt(msg, sizeof(msg), T("S_HATCH_FIRST", "Hatch the {egg} first")); break;
    case StoreError::Busy: fmt(msg, sizeof(msg), T("S_SG_ERR_BUSY", "Not right now")); break;
    case StoreError::TooYoung:
    case StoreError::NotAwakened: storeGateText(e, msg, sizeof(msg)); break;
    case StoreError::Full: fmt(msg, sizeof(msg), T("S_STORAGE_FULL", "{storage} is full")); break;
    default: fmt(msg, sizeof(msg), T("S_SG_ERR", "Can't do that")); break;
  }
  banner(msg, now);
  plat::sfx(plat::SFX_DENY);
}

static void succeeded(uint32_t now) {
  effectUntil = tama::deadlineIn(now, kEffectMs);
  plat::sfx(plat::SFX_EVOLVE);
}

bool storageAskSwitch(uint16_t species, bool variant, uint32_t now) {
  storageEnter();
  int16_t slot = pet->findStored(species, variant);
  if (slot < 0) return false;
  detail = slot;
  StoreError can = pet->canSwitchWith((uint8_t)slot);
  if (can == StoreError::Ok || can == StoreError::Duplicate) {
    dialog = Dialog::Switch;   // the same dialog (and duplicate/replace flow) as the Gap's SWITCH
    dialogSlot = slot;
    return true;
  }
  errorBanner(can, now);
  return false;
}

// --------------------------------------------------------------------------- drawing

static void drawEffect(uint32_t now) {
  // Procedural "gap": a slit opens across the screen, the scene shrinks into it, then a flash.
  float t = 1.0f - (float)tama::timeLeft(effectUntil, now) / kEffectMs;
  int h = t < 0.5f ? (int)(t * 2 * 160) : (int)((1.0f - t) * 2 * 160);
  gfx->fillRect(0, 0, kScreenW, kScreenH, 0x0000);
  gfx->fillRoundRect(CX - 200, CY - h / 2, 400, h > 2 ? h : 2, h / 2 + 1, tama::kPal_accent_default);
  gfx->fillRoundRect(CX - 196, CY - h / 2 + 3, 392, h > 8 ? h - 6 : 2, h / 2 + 1, 0x18C4);
  for (int i = 0; i < 5 && h > 30; i++) {   // eyes in the gap (Touhou's Yukari gaps); any theme: dots
    int ex = CX - 140 + i * 70, ey = CY + (int)(sinf(now * 0.01f + i) * h / 6);
    gfx->fillCircle(ex, ey, 6, 0xFFFF);
    gfx->fillCircle(ex, ey, 3, tama::kPal_bar_bad);
  }
  if (t > 0.85f) gfx->fillCircle(CX, CY, (int)(260 * (t - 0.85f) / 0.15f), 0xFFFF);
}

static void renderGrid(const Palette &p) {
  char title[48];
  fmt(title, sizeof(title), "{storage}");
  snprintf(title + strlen(title), sizeof(title) - strlen(title), " %u/%u", pet->ownedCount(), pet->storageCapacity());
  drawTitle(title, p.ink);
  int cells = cellCount();
  if (!pet->storageCount()) {
    fmt(title, sizeof(title), T("S_STORAGE_EMPTY", "Nothing stored yet"));
    textCentered(title, pet->isEgg() ? 150 : 214, 2, p.ink);
    textCentered(T("S_SG_EMPTY_HINT", "Evolving leaves a keepsake here"), pet->isEgg() ? 182 : 246, 1, p.ink);
  }
  int first = router.page() * kCols * kRows;
  for (int k = 0; k < kCols * kRows; k++) {
    int cell = first + k;
    if (cell >= cells) break;
    int cx = kGridX + (k % kCols) * kCell + kCell / 2, cy = kGridY + (k / kCols) * kCell + kCell / 2;
    if (cell < activeCells()) {   // the active pet: accent border + "YOU", not selectable
      gfx->drawRoundRect(cx - kCell / 2 + 3, cy - kCell / 2 + 3, kCell - 6, kCell - 6, 12, tama::kPal_accent_default);
      gfx->drawRoundRect(cx - kCell / 2 + 4, cy - kCell / 2 + 4, kCell - 8, kCell - 8, 11, tama::kPal_accent_default);
      drawThumb(pet->species(), cx, cy, 2, pet->variant(), false);
      levelBadge(cx, cy, pet->level(), 0xFFFF);
      const char *you = T("S_SG_WITH_YOU", "YOU");
      gfx->fillRoundRect(cx - textW(you, 1) / 2 - 3, cy + kCell / 2 - 16, textW(you, 1) + 6, 11, 3, tama::kPal_accent_default);
      text(you, cx - textW(you, 1) / 2, cy + kCell / 2 - 14, 1, 0xFFFF);
      if (pet->variant()) text("*", cx + 24, cy - 36, 2, tama::kPal_bar_warn);
      continue;
    }
    const tama::Specimen &s = pet->stored((uint8_t)(cell - activeCells()));
    gfx->drawRoundRect(cx - kCell / 2 + 3, cy - kCell / 2 + 3, kCell - 6, kCell - 6, 12, p.dim);
    drawThumb(s.species, cx, cy, 2, s.variant, false);
    levelBadge(cx, cy, pet->levelOf(s), 0xFFFF);
    if (s.variant) text("*", cx + 24, cy - 36, 2, tama::kPal_bar_warn);
  }
  // store the active pet & start a new egg (gated by the engine: evolved, or time with you)
  StoreError can = pet->canStoreActive();
  if (!pet->isEgg()) {
    bool ok = can == StoreError::Ok || can == StoreError::Duplicate;
    gfx->fillRoundRect(CX - kStoreBtnW / 2, kStoreBtnY, kStoreBtnW, kStoreBtnH, 14, ok ? tama::kPal_accent_default : p.dim);
    char label[48];
    fmt(label, sizeof(label), T("S_STORE_BTN", "Store + new {egg}"), displayName());
    textCentered(label, kStoreBtnY + (textW(label, 2) <= kStoreBtnW - 16 ? 13 : 17), textW(label, 2) <= kStoreBtnW - 16 ? 2 : 1,
                 ok ? 0xFFFF : p.ink);
    if (can == StoreError::TooYoung || can == StoreError::NotAwakened) {
      storeGateText(can, label, sizeof(label));
      textCentered(label, kStoreBtnY + 48, 1, p.ink);
    }
  } else {
    fmt(title, sizeof(title), T("S_HATCH_FIRST", "Hatch the {egg} first"));
    textCentered(title, kStoreBtnY + 14, 1, p.ink);
  }
  drawPageDots(storagePages(), router.page(), p.ink, p.dim);
}

static void renderDetail(uint32_t now, const Palette &p) {
  const tama::Specimen &s = pet->stored((uint8_t)detail);
  const tama::SpeciesDef &d = tama::kTheme.species[s.species];
  char line[64];
  drawTitle(specimenName(s), d.accent565);
  if (!detailArt.ok) {
    snprintf(line, sizeof(line), "/sprites/%s.spr", d.id);
    detailArt.load(line);
  }
  if (detailArt.ok)
    drawSprite(detailArt.pack, tama::SLOT_SLEEP, detailArt.pack.frameAt(tama::SLOT_SLEEP, now), CX, 196,
               spriteScale(detailArt.pack, 120), s.variant);   // asleep in stasis
  snprintf(line, sizeof(line), "Lv %u%s%s", pet->levelOf(s), s.variant ? "  " : "", s.variant ? T("V_VARIANT", "RARE") : "");
  textCentered(line, 206, 2, p.ink);
  if (s.origin == tama::Origin::Keepsake) {
    const char *of = s.keptFromNick[0] ? s.keptFromNick : speciesName(s.keepsakeOf);
    fmt(line, sizeof(line), T("S_KEPT_FROM", "Keepsake of {name}"), of);
  } else {
    fmt(line, sizeof(line), T("S_ORIGIN_HATCHED", "Hatched"));
  }
  textCentered(line, 230, 1, p.ink);
  const uint8_t vals[4] = {s.food, s.joy, s.energy, s.hygiene};
  for (int i = 0; i < 4; i++) drawBarH(CX - 130 + (i % 2) * 140, 250 + (i / 2) * 18, 120, 12, vals[i], 100, needColour(vals[i]), p.dim);
  StoreError can = pet->canSwitchWith((uint8_t)detail);
  bool ok = can == StoreError::Ok || can == StoreError::Duplicate;
  gfx->fillRoundRect(CX - 110, 300, 220, 44, 14, ok ? tama::kPal_accent_default : p.dim);
  textCentered(T("S_SWITCH_BTN", "SWITCH"), 314, 2, ok ? 0xFFFF : p.ink);
  textCentered(T("S_SG_RELEASE_HINT", "hold to release"), 356, 1, p.dim);
}

static void renderDialog(const Palette &p) {
  char q[96];
  const tama::Specimen *s = dialogSlot >= 0 && dialogSlot < pet->storageCount() ? &pet->stored((uint8_t)dialogSlot) : nullptr;
  switch (dialog) {
    case Dialog::Switch: {
      FmtArgs a;
      a.name = displayName();
      a.other = s ? specimenName(*s) : "?";
      fmt(q, sizeof(q), T("S_SWITCH_Q", "Send {name} to {storage} and bring out {other}?"), a);
      drawDialog(q, T("S_YES", "Yes"), T("S_NO", "No"), p.ink);
      break;
    }
    case Dialog::Store: {
      fmt(q, sizeof(q), T("S_STORE_Q", "Send {name} to {storage} and start a new {egg}?"), displayName());
      drawDialog(q, T("S_YES", "Yes"), T("S_NO", "No"), p.ink);
      break;
    }
    case Dialog::ReplaceSwitch:
    case Dialog::ReplaceStore: {
      int16_t dup = pet->findStored(pet->species(), pet->variant());
      FmtArgs a;
      a.name = speciesName(pet->species());
      a.n = dup >= 0 ? pet->levelOf(pet->stored((uint8_t)dup)) : 0;
      fmt(q, sizeof(q), T("S_REPLACE_Q", "Replace the stored {name} (Lv {n})?"), a);
      drawDialog(q, T("S_YES", "Yes"), T("S_NO", "No"), p.ink);
      break;
    }
    case Dialog::Release: {
      fmt(q, sizeof(q), T("S_RELEASE_STORED_Q", "Release {name}?"), s ? specimenName(*s) : "?");
      drawDialog(q, T("S_YES", "Yes"), T("S_NO", "No"), p.ink);
      break;
    }
    case Dialog::None:
      break;
  }
}

void storageRender(uint32_t now) {
  if (tama::timeLeft(effectUntil, now)) {
    drawEffect(now);
    return;
  }
  if (effectUntil) {   // effect finished: back to the pet
    effectUntil = 0;
    storageEnter();
    router.go(nav::Screen::Main, now);
    return;
  }
  Palette p = palette();
  fillBackground(p.bg);
  drawBackMark(p.ink);
  if (detail >= pet->storageCount()) detail = -1;   // slots shift on insert/remove
  if (detail >= 0) renderDetail(now, p);
  else renderGrid(p);
  renderDialog(p);
}

// --------------------------------------------------------------------------- input

static void doSwitch(bool replace, uint32_t now) {
  StoreError e = pet->switchWith((uint8_t)dialogSlot, now, replace);
  if (e == StoreError::Duplicate && !replace) { dialog = Dialog::ReplaceSwitch; return; }
  dialog = Dialog::None;
  if (e == StoreError::Ok) succeeded(now);
  else errorBanner(e, now);
}

static void doStore(bool replace, uint32_t now) {
  StoreError e = pet->storeActiveAndNewEgg(now, replace);
  if (e == StoreError::Duplicate && !replace) { dialog = Dialog::ReplaceStore; return; }
  dialog = Dialog::None;
  if (e == StoreError::Ok) succeeded(now);
  else errorBanner(e, now);
}

void storageTap(int x, int y, uint32_t now) {
  if (tama::timeLeft(effectUntil, now)) return;
  if (dialog != Dialog::None) {
    uint8_t h = dialogHit(x, y);
    if (!h) return;
    bool yes = h == 1;
    switch (dialog) {
      case Dialog::Switch: if (yes) doSwitch(false, now); else dialog = Dialog::None; break;
      case Dialog::ReplaceSwitch: if (yes) doSwitch(true, now); else dialog = Dialog::None; break;
      case Dialog::Store: if (yes) doStore(false, now); else dialog = Dialog::None; break;
      case Dialog::ReplaceStore: if (yes) doStore(true, now); else dialog = Dialog::None; break;
      case Dialog::Release:
        if (yes && dialogSlot >= 0) { pet->releaseStored((uint8_t)dialogSlot); plat::sfx(plat::SFX_BYE); detail = -1; }
        dialog = Dialog::None;
        break;
      case Dialog::None: break;
    }
    return;
  }
  if (detail >= 0) {
    if (hit(x, y, CX - 110, 300, 220, 44)) {
      StoreError can = pet->canSwitchWith((uint8_t)detail);
      if (can == StoreError::Ok || can == StoreError::Duplicate) { dialog = Dialog::Switch; dialogSlot = detail; }
      else errorBanner(can, now);
    }
    return;
  }
  if (!pet->isEgg() && hit(x, y, CX - kStoreBtnW / 2, kStoreBtnY, kStoreBtnW, kStoreBtnH)) {
    StoreError can = pet->canStoreActive();
    if (can == StoreError::Ok || can == StoreError::Duplicate) dialog = Dialog::Store;
    else errorBanner(can, now);
    return;
  }
  if (x < kGridX || y < kGridY) return;   // check before dividing (TamaPoke 1.10)
  int col = (x - kGridX) / kCell, row = (y - kGridY) / kCell;
  if (col >= kCols || row >= kRows) return;
  int cell = router.page() * kCols * kRows + row * kCols + col;
  if (cell < activeCells()) {
    char msg[48];
    fmt(msg, sizeof(msg), T("S_SG_ACTIVE_HINT", "{name} is with you"), displayName());
    banner(msg, now, 1500);
    return;
  }
  int slot = cell - activeCells();
  if (slot < pet->storageCount()) {
    detail = (int16_t)slot;
    detailArt.unload();
  }
}

void storageLongPress(int x, int y, uint32_t now) {
  (void)now;
  if (dialog != Dialog::None) return;
  int slot = -1;
  if (detail >= 0) slot = detail;
  else if (x >= kGridX && y >= kGridY) {
    int col = (x - kGridX) / kCell, row = (y - kGridY) / kCell;
    if (col < kCols && row < kRows) slot = router.page() * kCols * kRows + row * kCols + col - activeCells();
  }
  if (slot >= 0 && slot < pet->storageCount()) {
    dialog = Dialog::Release;
    dialogSlot = (int16_t)slot;
  }
}

bool storageSwipe(nav::Swipe s) {
  if (dialog != Dialog::None) return true;    // dialogs swallow swipes
  if (detail < 0) return false;
  if (s == nav::Swipe::Right) { detail = -1; detailArt.unload(); }   // leaving swipe closes the detail
  return true;
}

bool storageBack() {
  if (dialog != Dialog::None) { dialog = Dialog::None; return true; }
  if (detail < 0) return false;
  detail = -1;
  detailArt.unload();
  return true;
}

}  // namespace ui
