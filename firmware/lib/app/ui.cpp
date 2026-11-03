// Screen router + the main pet screen (and its egg, minigame and first-run starter views).
// Hub screens live in ui_status / ui_chronicle / ui_storage / ui_hub (settings); shared
// helpers in ui_common. Navigation rules: lib/app/nav.* (host-tested).
#include "ui.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui_common.h"
#include "ui_screens.h"

namespace ui {

using app::gfx;
using nav::Screen;
using tama::Gesture;
using tama::Mood;

constexpr int kBtnHalf = 26, kBtnHit = 36;
struct Btn { int16_t cx, cy; };
constexpr Btn kButtons[4] = {{140, 390}, {202, 404}, {264, 404}, {326, 390}};   // feed play sleep clean

enum class Overlay : uint8_t { None, Food, ConfirmRelease, ChoiceEvolve, ChoiceFarewell };
static Overlay overlay = Overlay::None;
static uint32_t overlayUntil = 0;

static Asset petArt, prevArt, bgArt, eggArt[3], starterArt[3];
static Asset itemFood[3], itemTreat, itemMess, itemEmote, itemPlay, icons[4];
static int16_t petArtFor = -2;
static bool petVariantFor = false;
static char pendingToast[64];   // shown once the evolution FX has finished

// Egg picker (Phase::Complete: everything collected, the player chooses the next egg).
static Asset pickArt;
static int16_t pickArtFor = -1;
static uint16_t pickPos = 0;     // index among the base forms
static bool pickVariant = false;

static struct {   // wandering on the main screen
  uint8_t mode = 0;   // 0 idle, 1 walk, 2 one-shot happy
  float x = CX, targetX = CX;
  uint32_t t0 = 0, until = 0;
} beh;

static struct {   // play minigame (keepy-uppy with the theme's play object)
  float x, y, vx, vy;
  uint16_t score;
  uint8_t misses;
  uint32_t last, overUntil;
} game;

// --------------------------------------------------------------------------- art

static void loadItem(Asset &a, const char *name) {
  char p[40];
  snprintf(p, sizeof(p), "/items/%s.spr", name);
  a.load(p);
}

static void ensurePetArt() {
  int16_t sp = pet->isEgg() ? -1 : (int16_t)pet->species();
  if (sp == petArtFor && pet->variant() == petVariantFor) return;
  petArtFor = sp;
  petVariantFor = pet->variant();
  beh = {};
  if (sp < 0) {
    petArt.unload();
    char p[48];   // the egg sits in the theme's first habitat
    snprintf(p, sizeof(p), "/bg/%s.spr", tama::themeHabitat(0));
    bgArt.load(p);
    return;
  }
  char p[48];
  snprintf(p, sizeof(p), "/sprites/%s.spr", tama::kTheme.species[sp].id);
  petArt.load(p);
  snprintf(p, sizeof(p), "/bg/%s.spr", tama::themeHabitat(tama::kTheme.species[sp].habitat));
  bgArt.load(p);
}

void begin(tama::Pet &p) {
  pet = &p;
  for (int i = 0; i < 3; i++) {
    char n[8];
    snprintf(n, sizeof(n), "food_%d", i);
    loadItem(itemFood[i], n);
    snprintf(n, sizeof(n), "egg_%d", i);
    loadItem(eggArt[i], n);
  }
  loadItem(itemTreat, "treat");
  loadItem(itemMess, "mess");
  loadItem(itemEmote, "emote");
  loadItem(itemPlay, "play_object");
  const char *iconNames[4] = {"icon_feed", "icon_play", "icon_sleep", "icon_clean"};
  for (int i = 0; i < 4; i++) loadItem(icons[i], iconNames[i]);
  router = nav::Router();
  router.setHintsSeen(pet->uiFlags() & 0xF);
  petArtFor = -2;
  if (pet->awaitingStarter()) {
    router.go(Screen::Starter, 0);
    for (uint8_t i = 0; i < tama::kTheme.starterCount && i < 3; i++) {
      char path[48];
      snprintf(path, sizeof(path), "/sprites/%s.spr", tama::kTheme.species[tama::kTheme.starters[i]].id);
      starterArt[i].load(path);
    }
  } else {
    router.go(Screen::Main, 0);
  }
  plat::setSoundEnabled(!(pet->uiFlags() & (1u << 5)));
  plat::setBrightnessLevel(pet->brightness());
}

// --------------------------------------------------------------------------- main screen

static void drawScene(bool night) {
  if (bgArt.ok) {
    drawSprite(bgArt.pack, tama::SLOT_IDLE, 0, CX, kPanelY, 2, false, false, night ? 150 : 255);
    gfx->fillRect(0, 0, 1, kPanelY, tama::kPal_bg_night);        // the 2 px the 464-wide art misses
    gfx->fillRect(465, 0, 1, kPanelY, tama::kPal_bg_night);
  } else {   // no art: flat sky + ground in the theme's colours
    gfx->fillRect(0, 0, kScreenW, kGround - 40, night ? tama::kPal_bg_night : tama::kPal_bg_day);
    gfx->fillRect(0, kGround - 40, kScreenW, kPanelY - (kGround - 40), shade565(tama::kPal_track, night ? 120 : 255));
  }
  gfx->fillRect(0, kPanelY, kScreenW, kScreenH - kPanelY, night ? tama::kPal_bg_night : tama::kPal_bg_day);
}

static const char *statusLine(uint32_t now) {
  if (pet->evolving(now)) return T("S_EVOLVING", "...!?");
  if (pet->sleeping()) return "Zzz...";
  if (pet->eating(now)) return T("S_EATING", "Munch munch");
  if (pet->showHeart(now)) return T("S_LIKES", "Loves it!");
  if (pet->food() < 25) return T("S_HUNGRY", "Hungry...");
  if (pet->hygiene() < 25) return T("S_NEEDS_CLEAN", "Needs a clean");
  if (pet->energy() < 25) return T("S_EXHAUSTED", "Exhausted...");
  if (pet->joy() < 25) return T("S_SAD", "Sad...");
  if (pet->weight() > 60) return T("S_CHUBBY", "A bit chubby");
  if (pet->variant() && pet->ageMinutes() < 15) return T("S_IS_VARIANT", "Rare colours!");
  return T("S_HAPPY", "Happy");
}

static void drawHeader(uint32_t now, uint16_t ink, uint16_t outline) {
  char line[40];
  if (pet->isEgg()) {
    fmt(line, sizeof(line), T("S_EGG_HDR", "{egg}"));
    textOutlined(line, 40, 2, ink, outline);
    const char *msg = pet->eggTaps() == 0 ? T("S_EGG_TOUCH", "Tap the {egg}")
                    : pet->eggTaps() == 1 ? T("S_EGG_MOVES", "It moved!") : T("S_EGG_ALMOST", "Almost!");
    fmt(line, sizeof(line), msg);
    textOutlined(line, 64, 2, ink, outline);
    return;
  }
  snprintf(line, sizeof(line), "%s%s Lv.%u", pet->variant() ? "*" : "", displayName(), pet->level());
  textOutlined(line, 40, 2, pet->speciesDef()->accent565, outline);
  textOutlined(statusLine(now), 64, 2, ink, outline);
}

// Animation slot from the mood, with TamaPoke-style wandering when happy. Missing slots fall
// back to a procedural animation of idle (docs/themes.md).
static void drawPet(uint32_t now) {
  if (!petArt.ok) {
    textCentered("?", kGround - 120, 6, tama::kPal_ink);
    return;
  }
  const tama::SpritePack &sp = petArt.pack;
  int scale = spriteScale(sp);
  uint8_t slot = tama::SLOT_IDLE, shade = 255;
  int dy = 0;
  bool mirror = false, loop = true;
  uint32_t t = now - beh.t0;
  float speed = 1.0f;
  switch (pet->mood(now)) {
    case Mood::Sleeping:
      slot = tama::SLOT_SLEEP;
      if (!sp.has(slot)) { shade = 150; speed = 0.4f; }
      beh.mode = 0;
      break;
    case Mood::Eating:
      slot = tama::SLOT_EAT;
      if (!sp.has(slot)) { speed = 2.0f; dy = ((now / 120) & 1) ? 2 : 0; }
      break;
    case Mood::Sad:
      slot = tama::SLOT_SAD;
      if (!sp.has(slot)) { speed = 0.5f; dy = 2 * scale; shade = 200; }
      break;
    case Mood::Happy:
      if ((int32_t)(now - beh.until) > 0) {   // pick the next whim
        beh.t0 = now;
        uint32_t r = plat::rnd(100);
        if (r < 35) { beh.mode = 1; beh.targetX = 150 + plat::rnd(166); beh.until = now + 15000; }
        else if (r < 50 && sp.has(tama::SLOT_HAPPY)) { beh.mode = 2; beh.until = now + 1500; }
        else { beh.mode = 0; beh.until = now + 2000 + plat::rnd(3000); }
        t = 0;
      }
      if (beh.mode == 1) {
        float d = beh.targetX - beh.x;
        if (fabsf(d) < 4) { beh.mode = 0; beh.until = now + 2500; }
        else {
          beh.x += d > 0 ? 3.0f : -3.0f;
          slot = tama::SLOT_WALK;
          mirror = d < 0;                       // walk strips face right
          if (!sp.has(slot)) dy = ((now / 150) & 1) ? 2 : 0;
        }
      } else if (beh.mode == 2) {
        slot = tama::SLOT_HAPPY;
        loop = false;
      }
      if (pet->showHeart(now) && !sp.has(tama::SLOT_HAPPY)) dy = -(int)(6 * fabsf(sinf(now * 0.012f)));
      break;
  }
  uint8_t useSlot = sp.has(slot) ? slot : (uint8_t)tama::SLOT_IDLE;
  uint8_t frame = sp.frameAt(useSlot, (uint32_t)(t * speed), loop);
  drawSprite(sp, useSlot, frame, (int)beh.x, kGround + dy, scale, pet->variant(), mirror, shade);
  int top = kGround - sp.slot(tama::SLOT_IDLE).h * scale;
  if (pet->sleeping()) text("Zz", (int)beh.x + 50, top - 10, 3, tama::kPal_ink_night);
  if (pet->showHeart(now) && itemEmote.ok) drawItem(itemEmote, (int)beh.x + 56, top - 8, 32);
}

static void drawEvolveFx(uint32_t now) {
  float t = pet->evolveProgress(now);
  int cy = kGround - 90;
  int halo = 36 + (int)(t * 150) + (int)(8 * sinf(now * 0.02f));
  for (int k = 0; k < 4; k++)
    if (halo - k * 7 > 0) gfx->drawCircle(CX, cy, halo - k * 7, 0xFFFF);
  for (int i = 0; i < 12; i++) {
    float a = now * 0.004f + i * (float)(3.14159265f / 6);
    int len = 90 + (int)(70 * (0.5f + 0.5f * sinf(now * 0.012f + i)));
    gfx->drawLine(CX, cy, CX + (int)(cosf(a) * len), cy + (int)(sinf(a) * len), 0xFFFF);
  }
  // flicker between the old and new silhouette, speeding up; reveal at the end
  int period = 60 + (int)(220 * (1.0f - t));
  bool old = t < 0.9f && prevArt.ok && ((now / period) % 2) == 0;
  const Asset &a = old ? prevArt : petArt;
  if (a.ok) drawSprite(a.pack, tama::SLOT_IDLE, 0, CX, kGround, spriteScale(a.pack), false, false, 255, 0x18C4);
  if (t > 0.9f) gfx->fillCircle(CX, cy, (int)(300 * (t - 0.9f) / 0.1f), 0xFFFF);
}

static void drawBars(uint16_t ink) {
  const uint8_t vals[4] = {pet->food(), pet->joy(), pet->energy(), pet->hygiene()};
  const char *fallback[4] = {"FOOD", "JOY", "ENE", "HYG"};
  for (int i = 0; i < 4; i++) {
    char key[16];
    snprintf(key, sizeof(key), "V_NEEDS_%d", i);
    const char *label = T(key, fallback[i]);
    int x = (i % 2) ? 244 : 78, y = (i / 2) ? 346 : 318;
    text(label, x, y, 2, ink);
    int gap = textW(label, 2) + 8;
    if (gap < 48) gap = 48;
    int bw = 232 - (78 + gap);
    if (bw > 100) bw = 100;
    if (bw < 40) bw = 40;
    drawBarH(x + gap, y, bw, 15, vals[i], 100, needColour(vals[i]), tama::kPal_track);
  }
}

static void drawButtons(uint16_t ink) {
  for (int i = 0; i < 4; i++) {
    bool off = pet->sleeping() && i != 2;   // asleep: only the sleep/wake button works
    int bx = kButtons[i].cx - kBtnHalf, by = kButtons[i].cy - kBtnHalf;
    if (!pet->sleeping()) gfx->fillRoundRect(bx, by, 2 * kBtnHalf, 2 * kBtnHalf, 14, 0xFFFF);
    gfx->drawRoundRect(bx, by, 2 * kBtnHalf, 2 * kBtnHalf, 14, ink);
    if (!off) drawItem(icons[i], kButtons[i].cx, kButtons[i].cy, 34);
  }
}

static void drawCta(uint16_t ink) {
  char line[48];
  if (pet->wantEvolveButton()) {
    fmt(line, sizeof(line), T("S_EVO_TAP", "{evolve_verb}!"));
    gfx->fillRoundRect(CX - 128, 172, 256, 56, 16, tama::kPal_accent_default);
    textCentered(line, 192, 2, 0xFFFF);
  } else if (pet->canRunaway()) {
    fmt(line, sizeof(line), T("S_RUNAWAY_BTN", "{name} is leaving!"), displayName());
    gfx->fillRoundRect(CX - 200, 176, 400, 50, 14, tama::kPal_bg_night);
    textCentered(line, 194, 2, 0xFFFF);
  } else if (pet->wantFarewellButton()) {
    fmt(line, sizeof(line), T("S_FAREWELL_BTN", "{name} wants to go"), displayName());
    gfx->fillRoundRect(CX - 200, 176, 400, 50, 14, tama::kPal_bar_warn);
    textCentered(line, 194, 2, ink);
  }
}

static void drawOverlay(uint32_t now, uint16_t ink) {
  if (overlay != Overlay::None && overlayUntil && !tama::timeLeft(overlayUntil, now)) overlay = Overlay::None;
  char q[64];
  const char *name = pet->isEgg() ? "" : displayName();
  switch (overlay) {
    case Overlay::None:
      break;
    case Overlay::Food:
      gfx->fillRoundRect(101, 260, 264, 72, 14, 0xFFFF);
      gfx->drawRoundRect(101, 260, 264, 72, 14, ink);
      for (int i = 0; i < 3; i++) drawItem(itemFood[i], 134 + i * 66, 296, 48);
      drawItem(itemTreat, 134 + 3 * 66, 296, 48);
      break;
    case Overlay::ConfirmRelease:
      fmt(q, sizeof(q), T("S_RELEASE_FMT", "Release {name}?"), name);
      drawDialog(q, T("S_YES", "Yes"), T("S_NO", "No"), ink);
      break;
    case Overlay::ChoiceEvolve: {
      char yes[24];
      fmt(q, sizeof(q), T("S_EVO_Q", "{evolve_verb} {name}?"), name);
      fmt(yes, sizeof(yes), T("V_EVOLVE_VERB", "EVOLVE"));
      drawDialog(q, yes, T("S_EVO_KEEP", "Not yet"), ink);
      break;
    }
    case Overlay::ChoiceFarewell:
      fmt(q, sizeof(q), T("S_FAR_Q", "Let {name} go?"), name);
      drawDialog(q, T("S_FAR_GO", "Goodbye"), T("S_FAR_STAY", "Stay"), ink);
      break;
  }
}

// --------------------------------------------------------------------------- egg picker

static uint16_t baseFormCount() {
  uint16_t n = 0;
  for (uint16_t i = 0; i < tama::kTheme.speciesCount; i++) n += tama::kTheme.species[i].tier != tama::kTierEvo;
  return n;
}

static uint16_t baseFormAt(uint16_t pos) {
  for (uint16_t i = 0; i < tama::kTheme.speciesCount; i++)
    if (tama::kTheme.species[i].tier != tama::kTierEvo && pos-- == 0) return i;
  return 0;
}

constexpr int kPickPrevX = 20, kPickNextX = 356, kPickArrowY = 110, kPickArrowW = 90, kPickArrowH = 130;
constexpr int kPickColourY = 286, kPickHatchY = 338, kPickBtnW = 220;

static void renderEggPick(uint32_t now, const Palette &p) {
  char line[48];
  fmt(line, sizeof(line), T("S_PICK_EGG", "Choose your next {egg}"));
  textOutlined(line, 40, 2, 0xFFFF, p.outline);
  uint16_t n = baseFormCount();
  if (pickPos >= n) pickPos = 0;
  uint16_t sp = baseFormAt(pickPos);
  if (pickArtFor != (int16_t)sp) {
    char path[48];
    snprintf(path, sizeof(path), "/sprites/%s.spr", tama::kTheme.species[sp].id);
    pickArt.load(path);
    pickArtFor = (int16_t)sp;
  }
  if (pickArt.ok)
    drawSprite(pickArt.pack, tama::SLOT_IDLE, pickArt.pack.frameAt(tama::SLOT_IDLE, now), CX, 240,
               spriteScale(pickArt.pack, 150), pickVariant);
  for (int side = 0; side < 2; side++) {   // < > on dark discs, readable on any background
    int ax = side ? kPickNextX + kPickArrowW / 2 : kPickPrevX + kPickArrowW / 2, ay = kPickArrowY + kPickArrowH / 2;
    gfx->fillCircle(ax, ay, 28, p.outline);
    text(side ? ">" : "<", ax - 12, ay - 17, 5, 0xFFFF);
  }
  snprintf(line, sizeof(line), "%s%s", pickVariant ? "*" : "", tama::kTheme.species[sp].name);
  textOutlined(line, 252, 2, tama::kTheme.species[sp].accent565, p.outline);
  gfx->fillRect(0, kPanelY, kScreenW, kScreenH - kPanelY, p.night ? tama::kPal_bg_night : tama::kPal_bg_day);
  gfx->fillRoundRect(CX - kPickBtnW / 2, kPickColourY, kPickBtnW, 40, 12, pickVariant ? tama::kPal_bar_warn : 0xFFFF);
  textCentered(pickVariant ? T("V_VARIANT", "RARE COLOUR") : T("S_PICK_NORMAL", "NORMAL"), kPickColourY + 12, 2,
               0x2946);   // dark ink on both (white / warm yellow) buttons
  gfx->fillRoundRect(CX - kPickBtnW / 2, kPickHatchY, kPickBtnW, 50, 14, tama::kPal_accent_default);
  textCentered(T("S_PICK_HATCH", "HATCH"), kPickHatchY + 16, 2, 0xFFFF);
  snprintf(line, sizeof(line), "%u/%u", pickPos + 1, n);
  textCentered(line, kPickHatchY + 62, 1, p.ink);
}

static void tapEggPick(int x, int y) {
  uint16_t n = baseFormCount();
  if (!n) return;
  if (hit(x, y, kPickPrevX, kPickArrowY, kPickArrowW, kPickArrowH)) {
    pickPos = (uint16_t)((pickPos + n - 1) % n);
    plat::sfx(plat::SFX_TAP);
  } else if (hit(x, y, kPickNextX, kPickArrowY, kPickArrowW, kPickArrowH)) {
    pickPos = (uint16_t)((pickPos + 1) % n);
    plat::sfx(plat::SFX_TAP);
  } else if (hit(x, y, CX - kPickBtnW / 2, kPickColourY, kPickBtnW, 40)) {
    pickVariant = !pickVariant;
    plat::sfx(plat::SFX_TAP);
  } else if (hit(x, y, CX - kPickBtnW / 2, kPickHatchY, kPickBtnW, 50)) {
    if (pet->chooseEgg(baseFormAt(pickPos), pickVariant)) {
      pickArt.unload();
      pickArtFor = -1;
      plat::sfx(plat::SFX_HATCH);
    }
  }
}

static void renderStarter() {
  fillBackground(tama::kPal_bg_day);
  textCentered(T("S_CHOOSE_STARTER", "Choose your partner"), 70, 2, tama::kPal_ink);
  for (uint8_t i = 0; i < tama::kTheme.starterCount && i < 3; i++) {
    int ry = 110 + i * 82;
    const tama::SpeciesDef &d = tama::kTheme.species[tama::kTheme.starters[i]];
    gfx->fillRoundRect(70, ry, 326, 72, 14, shade565(d.accent565, 90) | 0x8410);
    gfx->drawRoundRect(70, ry, 326, 72, 14, d.accent565);
    if (starterArt[i].ok) drawSprite(starterArt[i].pack, tama::SLOT_IDLE, 0, 120, ry + 68, 1);
    text(d.name, 178, ry + 26, 2, tama::kPal_ink);
  }
}

static void stepPlay(uint32_t now) {
  // physics on real elapsed time, not per frame (TamaPoke 1.9)
  float dt = (now - game.last) / 16.0f;
  if (dt > 4) dt = 4;
  game.last = now;
  if (!game.overUntil) {
    game.vy += 0.35f * dt;
    game.x += game.vx * dt;
    game.y += game.vy * dt;
    if (game.x < 90 || game.x > 376) game.vx = -game.vx;
    if (game.y > kGround - 20) {   // dropped it
      game.misses++;
      plat::sfx(plat::SFX_DENY);
      game.x = 150 + plat::rnd(166);
      game.y = 90;
      game.vy = 0;
      if (game.misses >= 3) game.overUntil = tama::deadlineIn(now, 2000);
    }
  } else if (!tama::timeLeft(game.overUntil, now)) {
    pet->playResult(game.score, now);
    router.go(Screen::Main, now);
  }
}

static void renderPlay(uint32_t now) {
  drawScene(false);
  char line[24];
  snprintf(line, sizeof(line), "%u", game.score);
  textOutlined(line, 40, 3, 0xFFFF, tama::kPal_ink);
  if (petArt.ok)
    drawSprite(petArt.pack, tama::SLOT_IDLE, petArt.pack.frameAt(tama::SLOT_IDLE, now), CX, kGround, spriteScale(petArt.pack), pet->variant());
  if (itemPlay.ok) drawItem(itemPlay, (int)game.x, (int)game.y, 48);
  else gfx->fillCircle((int)game.x, (int)game.y, 18, 0xFFFF);
  for (int i = 0; i < 3; i++) gfx->fillCircle(CX - 30 + i * 30, kPanelY + 40, 8, i < game.misses ? tama::kPal_bar_bad : tama::kPal_track);
  if (game.overUntil) textOutlined(T("S_GREAT", "Nice!"), 150, 4, 0xFFFF, tama::kPal_ink);
}

static void renderMain(uint32_t now) {
  Palette p = palette();
  drawScene(p.night);
  if (pet->isEgg() && pet->awaitingEggChoice()) {   // no edge hints: they'd sit on the arrows
    renderEggPick(now, p);
    return;
  }
  drawHeader(now, 0xFFFF, p.outline);
  if (pet->ending() != tama::Ending::None) {
    float t = pet->ceremonyProgress(now);
    if (petArt.ok) drawSprite(petArt.pack, tama::SLOT_IDLE, 0, CX, kGround - (int)(t * 40), spriteScale(petArt.pack),
                              pet->variant(), false, (uint8_t)(255 - t * 200));
    const char *key = pet->ending() == tama::Ending::Farewell ? "S_FAREWELL"
                    : pet->ending() == tama::Ending::Runaway ? "S_RUNAWAY" : "S_GOODBYE";
    textOutlined(T(key, "Goodbye"), 150, 2, 0xFFFF, 0x0000);
    return;
  }
  if (pet->isEgg()) {
    uint8_t k = pet->eggTaps() > 2 ? 2 : pet->eggTaps();
    if (eggArt[k].ok) drawItem(eggArt[k], CX, 200, 150);
    else gfx->fillCircle(CX, 200, 70, 0xFFFF);
    uint8_t tier = pet->eggTier();
    if (tier + 1 < tama::kTheme.tierCount) textOutlined(tama::themeTierHint(tier), 290, 2, tama::kPal_bar_warn, 0x0000);
  } else if (pet->evolving(now)) {
    drawEvolveFx(now);
  } else {
    if (prevArt.ok) prevArt.unload();
    if (pendingToast[0]) {            // keepsake / storage-full notice after the evolution FX
      banner(pendingToast, now);
      pendingToast[0] = 0;
    }
    for (int i = 0; i < pet->mess(); i++) drawItem(itemMess, 44 + i * 46, kGround - 16, 36);
    drawPet(now);
  }
  drawBars(p.ink);
  drawButtons(p.ink);
  if (!pet->isEgg() && overlay == Overlay::None) drawCta(p.ink);
  if (pet->showMedal(now)) textOutlined(T("S_MEDAL_BANNER", "MEDAL!"), 120, 3, tama::kPal_bar_warn, 0x0000);
  if (overlay == Overlay::None && !pet->evolving(now)) {
    drawBezelArc();
    drawEdgeHints(0xFFFF);
  }
  drawOverlay(now, p.ink);
}

// --------------------------------------------------------------------------- router glue

static bool busy(uint32_t now) {
  return overlay != Overlay::None || pet->evolving(now) || pet->ending() != tama::Ending::None ||
         storageBusy(now) || pet->awaitingStarter();
}

void render(uint32_t now) {
  if (router.tick(now)) {   // 30 s without a touch: back to the pet (burn-in, default view)
    chronicleEnter();
    storageEnter();
  }
  router.setPageCount(Screen::Status, statusPages());
  router.setPageCount(Screen::Chronicle, chroniclePages());
  router.setPageCount(Screen::Storage, storagePages());
  if (router.screen() != Screen::Starter) ensurePetArt();
  switch (router.screen()) {
    case Screen::Starter: renderStarter(); break;
    case Screen::Play: stepPlay(now); renderPlay(now); break;
    case Screen::Status: statusRender(now); break;
    case Screen::Chronicle: chronicleRender(now); break;
    case Screen::Storage: storageRender(now); break;
    case Screen::Settings: settingsRender(now); break;
    default: renderMain(now); break;
  }
  drawBanner(now);
  gfx->flush();
}

static bool inPet(int x, int y) { return x > 110 && x < 356 && y > 95 && y < 310; }

static void startPlay(uint32_t now) {
  if (pet->isEgg() || pet->sleeping()) return;
  game = {};
  game.x = 150 + plat::rnd(166);
  game.y = 90;
  game.vx = plat::rnd(2) ? 1.6f : -1.6f;
  game.last = now;
  router.go(Screen::Play, now);
}

static nav::Swipe toSwipe(Gesture::Kind k) {
  return k == Gesture::SwipeUp ? nav::Swipe::Up : k == Gesture::SwipeDown ? nav::Swipe::Down
       : k == Gesture::SwipeLeft ? nav::Swipe::Left : nav::Swipe::Right;
}

static void tapMain(int x, int y, uint32_t now) {
  if (overlay == Overlay::Food) {
    overlay = Overlay::None;
    if (hit(x, y, 101, 260, 264, 72)) {
      int i = (x - 101) / 66;
      if (i == 3) pet->feedTreat(now);
      else pet->feed((uint8_t)i, now);
      plat::sfx(plat::SFX_EAT);
    }
    return;
  }
  if (overlay != Overlay::None) {
    uint8_t h = dialogHit(x, y);
    bool yes = h == 1, no = h == 2;
    if (overlay == Overlay::ConfirmRelease && yes) pet->release(now);
    if (overlay == Overlay::ChoiceEvolve) {
      if (yes) {
        char p[48];
        snprintf(p, sizeof(p), "/sprites/%s.spr", pet->speciesDef()->id);
        prevArt.load(p);   // the old form, for the flicker
        pet->evolve(now);
      } else if (no) {
        pet->declineEvolve();
      }
    }
    if (overlay == Overlay::ChoiceFarewell) {
      if (yes) pet->startFarewell(now);
      else if (no) pet->declineFarewell();
    }
    if (yes || no) overlay = Overlay::None;
    return;
  }
  if (pet->ending() != tama::Ending::None) return;
  if (pet->isEgg() && pet->awaitingEggChoice()) {
    tapEggPick(x, y);
    return;
  }
  if (pet->isEgg()) {
    pet->eggTap(now);
    plat::sfx(plat::SFX_TAP);
    return;
  }
  if (pet->wantEvolveButton() && hit(x, y, CX - 128, 172, 256, 56)) {
    overlay = Overlay::ChoiceEvolve;
    overlayUntil = now + 12000;
    return;
  }
  if (hit(x, y, CX - 200, 176, 400, 50)) {
    if (pet->canRunaway()) { pet->startRunaway(now); return; }
    if (pet->wantFarewellButton()) { overlay = Overlay::ChoiceFarewell; overlayUntil = now + 12000; return; }
  }
  for (int i = 0; i < 4; i++) {
    int dx = x - kButtons[i].cx, dy = y - kButtons[i].cy;
    if (dx * dx + dy * dy > kBtnHit * kBtnHit) continue;
    plat::sfx(plat::SFX_TAP);
    if (i == 0 && !pet->sleeping()) { overlay = Overlay::Food; overlayUntil = now + 6000; }
    else if (i == 1) startPlay(now);
    else if (i == 2) pet->toggleSleep();
    else if (i == 3) pet->clean(now);
    return;
  }
  if (inPet(x, y)) pet->caress(now);
}

void onGesture(const Gesture &g, uint32_t now) {
  if (g.kind == Gesture::None) return;
  router.touched(now);
  Screen s = router.screen();

  if (s == Screen::Starter) {
    if (g.kind != Gesture::Tap) return;
    for (uint8_t i = 0; i < tama::kTheme.starterCount && i < 3; i++) {
      if (hit(g.x, g.y, 70, 110 + i * 82, 326, 72)) {
        pet->chooseStarter(i);
        plat::sfx(plat::SFX_TAP);
        for (auto &a : starterArt) a.unload();
        router.go(Screen::Main, now);
        return;
      }
    }
    return;
  }
  if (s == Screen::Play) {
    if (g.kind == Gesture::Tap && !game.overUntil) {
      float dx = g.x - game.x, dy = g.y - game.y;
      if (dx * dx + dy * dy < 60 * 60) {   // generous hit radius on a small round screen
        game.vy = -9.0f;
        game.vx = (game.x - g.x) * 0.08f + (plat::rnd(2) ? 1.2f : -1.2f);
        game.score++;
        plat::sfx(plat::SFX_PLAY);
      }
    } else if (g.kind == Gesture::SwipeDown) {
      router.go(Screen::Main, now);   // quit without a result
    }
    return;
  }

  if (g.kind == Gesture::LongPress) {
    if (s == Screen::Storage) storageLongPress(g.x, g.y, now);
    else if (s == Screen::Chronicle) chronicleLongPress(g.x, g.y, now);
    else if (s == Screen::Main && !pet->isEgg() && inPet(g.x, g.y) && pet->ending() == tama::Ending::None &&
             overlay == Overlay::None) {
      overlay = Overlay::ConfirmRelease;
      overlayUntil = now + 10000;
    }
    return;
  }

  if (g.kind != Gesture::Tap) {   // swipes: screen-internal first (close a detail), then the router
    nav::Swipe sw = toSwipe(g.kind);
    if (s == Screen::Chronicle && chronicleSwipe(sw)) return;
    if (s == Screen::Storage && storageSwipe(sw)) return;
    if (s == Screen::Main && overlay == Overlay::Food) overlay = Overlay::None;
    nav::Result r = router.onSwipe(sw, busy(now), now);
    if (r == nav::Result::Entered) {
      pet->setUiFlags((uint16_t)((pet->uiFlags() & ~0xFu) | (router.hintsSeen() & 0xFu)));
      if (router.screen() == Screen::Chronicle) chronicleEnter();
      if (router.screen() == Screen::Storage) storageEnter();
      plat::sfx(plat::SFX_TAP);
    } else if (r == nav::Result::Left || r == nav::Result::Paged) {
      plat::sfx(plat::SFX_TAP);
    }
    return;
  }

  // taps
  if (s != Screen::Main && backMarkHit(g.x, g.y)) {
    if (s == Screen::Chronicle && chronicleBack()) return;
    if (s == Screen::Storage && storageBack()) return;
    router.back(now);
    return;
  }
  switch (s) {
    case Screen::Status: statusTap(g.x, g.y, now); break;
    case Screen::Chronicle: chronicleTap(g.x, g.y, now); break;
    case Screen::Storage: storageTap(g.x, g.y, now); break;
    case Screen::Settings: settingsTap(g.x, g.y, now); break;
    default: tapMain(g.x, g.y, now); break;
  }
}

void onEvents(uint32_t ev) {
  using namespace tama;
  if (ev & EV_HATCH) plat::sfx(plat::SFX_HATCH);
  if (ev & EV_EVOLVE) plat::sfx(plat::SFX_EVOLVE);
  if (ev & (EV_MEDAL | EV_MILESTONE | EV_EVO_READY)) plat::sfx(plat::SFX_MEDAL);
  if (ev & EV_HEART) plat::sfx(plat::SFX_HEART);
  if (ev & EV_ENDING) plat::sfx(plat::SFX_BYE);
  if (ev & EV_RUNAWAY_READY) plat::sfx(plat::SFX_DENY);
  if (ev & EV_LEVEL_UP) plat::sfx(plat::SFX_LEVEL);
  if (ev & (EV_NEW_EGG | EV_SWITCH | EV_STORED)) { petArtFor = -2; beh = {}; }
  if (ev & EV_KEEPSAKE)   // arrives with EV_EVOLVE: shown once the evolution FX is over
    fmt(pendingToast, sizeof(pendingToast), T("S_KEEPSAKE_GOT", "{name} was kept in {storage}"),
        speciesName(pet->prevSpecies()));
  if (ev & EV_STORAGE_FULL) fmt(pendingToast, sizeof(pendingToast), T("S_STORAGE_FULL", "{storage} is full"));
  if (ev & EV_PHASE) {   // a collection finished: eggs now come in rare colours / by choice
    if (pet->phase() == Phase::Variant)
      fmt(pendingToast, sizeof(pendingToast), T("S_PHASE_VARIANT", "{collection} done! Rare colours next"));
    else if (pet->phase() == Phase::Complete)
      fmt(pendingToast, sizeof(pendingToast), T("S_PHASE_COMPLETE", "Everything found! Pick any {egg}"));
  }
  if (ev & EV_LOADED) {  // a save was imported: start the screens over on the new game
    overlay = Overlay::None;
    pendingToast[0] = 0;
    pickArt.unload();
    pickArtFor = -1;
    begin(*pet);
  }
}

bool busyAnimating(uint32_t now) {
  return router.screen() == Screen::Play || pet->evolving(now) || pet->ending() != tama::Ending::None ||
         pet->eating(now) || pet->showHeart(now) || storageBusy(now);
}

uint32_t frameIntervalMs() { return router.screen() == Screen::Play ? 85 : 100; }

uint32_t longPressMs() {
  Screen s = router.screen();
  return (s == Screen::Chronicle || s == Screen::Storage) ? 1000 : 3000;
}

}  // namespace ui
