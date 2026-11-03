// Status screen (swipe up): stats and how far the next milestones are.
// Pages (swipe left/right): Next up · Care · Growth · Bond & streak · Collection.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui_common.h"
#include "ui_screens.h"

namespace ui {

using app::gfx;
using tama::Milestone;
using tama::MilestoneKind;

static const char *kPageKeys[5] = {"S_PAGE_NEXT", "S_PAGE_CARE", "S_PAGE_GROWTH", "S_PAGE_BOND", "S_PAGE_COLLECTION"};
static const char *kPageFallback[5] = {"NEXT UP", "CARE", "GROWTH", "BOND & STREAK", "COLLECTION"};

uint8_t statusPages() { return pet->isEgg() ? 1 : 5; }

// --------------------------------------------------------------------------- milestones

static void milestoneLabel(const Milestone &m, char *out, size_t cap) {
  FmtArgs a;
  switch (m.kind) {
    case MilestoneKind::Hatch:
      fmt(out, cap, T("S_MS_HATCH", "Hatching"), a);
      break;
    case MilestoneKind::NextLevel:
      a.n = pet->level() + 1;
      fmt(out, cap, T("S_MS_LEVEL", "Lv {n}"), a);
      break;
    case MilestoneKind::Evolution: {
      a.n = (long)m.target;
      char tmp[48];
      fmt(tmp, sizeof(tmp), T("S_MS_EVOLVE", "{evolve_verb} at Lv {n}"), a);
      const char *into = m.ref == tama::kNoSpecies ? "?" : pet->isSeen(m.ref) ? speciesName(m.ref) : "???";
      if (textW(tmp, 2) + textW(into, 2) + 24 <= 302) snprintf(out, cap, "%s \x1a %s", tmp, into);
      else snprintf(out, cap, "%s", tmp);
      break;
    }
    case MilestoneKind::Farewell:
      fmt(out, cap, T("S_MS_FAREWELL", "Ready to say goodbye"), a);
      break;
    case MilestoneKind::Streak:
      a.n = (long)m.target;
      fmt(out, cap, T("S_MS_STREAK", "{n}-day streak"), a);
      break;
    case MilestoneKind::Bond:
      fmt(out, cap, T("S_MS_BOND", "Max bond"), a);
      break;
    case MilestoneKind::LevelMedal:
      a.n = (long)m.target;
      fmt(out, cap, T("S_MS_MEDAL", "Medal at Lv {n}"), a);
      break;
    case MilestoneKind::TierUnlock: {
      static char tier[24];   // "mythic" -> "MYTHIC"
      snprintf(tier, sizeof(tier), "%s", tama::kTheme.tiers[m.ref].id);
      for (char *c = tier; *c; c++) if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);
      a.tier = tier;
      fmt(out, cap, T("S_MS_TIER", "{tier} eggs unlock"), a);
      break;
    }
    case MilestoneKind::Collection:
      a.n = (long)m.current;
      a.max = (long)m.target;
      fmt(out, cap, T("S_MS_COLLECTION", "{storage} {n}/{max}"), a);
      break;
  }
}

static const char *unitSuffix(tama::MilestoneUnit u) {
  switch (u) {
    case tama::MilestoneUnit::Taps: return " taps";
    case tama::MilestoneUnit::Minutes: return " min";
    case tama::MilestoneUnit::Levels: return " Lv";
    case tama::MilestoneUnit::Days: return " d";
    case tama::MilestoneUnit::Points: return "";
    case tama::MilestoneUnit::Count: return "";
  }
  return "";
}

static void drawMilestone(const Milestone &m, int y, const Palette &p) {
  char label[48], line[64], eta[24];
  milestoneLabel(m, label, sizeof(label));
  text(label, 82, y, 2, p.ink);
  uint16_t fill = m.blocked ? tama::kPal_bar_warn : tama::kPal_accent_default;
  drawBarH(82, y + 22, 302, 14, m.current, m.target, fill, p.dim);
  if (m.etaMinutes == 0) snprintf(eta, sizeof(eta), "%s", T("S_MS_SOON", "now!"));
  else if (m.etaMinutes > 0) fmtDuration(eta, sizeof(eta), m.etaMinutes);
  else snprintf(eta, sizeof(eta), "-");
  if (m.unit == tama::MilestoneUnit::Minutes)
    snprintf(line, sizeof(line), "%s", eta);
  else
    snprintf(line, sizeof(line), "%u/%u%s  %s", (unsigned)m.current, (unsigned)m.target, unitSuffix(m.unit), m.etaMinutes >= 0 ? eta : "");
  text(line, 82, y + 42, 1, p.ink);
  if (m.blocked) {
    const char *why = (m.blocked & tama::MS_BLOCK_ASLEEP) ? T("S_MS_BLOCK_ASLEEP", "asleep")
                                                         : T("S_MS_BLOCK_NEEDS", "needs all 40+");
    text(why, 384 - textW(why, 1), y + 42, 1, tama::kPal_bar_warn);
  }
}

static void pageNext(const Palette &p) {
  Milestone ms[tama::kMaxMilestones];
  uint8_t n = pet->milestones(ms, tama::kMaxMilestones);
  if (!n) {
    textCentered(T("S_MS_NONE", "Nothing coming up"), 200, 2, p.ink);
    return;
  }
  for (uint8_t i = 0; i < n && i < 3; i++) drawMilestone(ms[i], 92 + i * 92, p);
}

// --------------------------------------------------------------------------- care

static void pageCare(const Palette &p) {
  const uint8_t vals[4] = {pet->food(), pet->joy(), pet->energy(), pet->hygiene()};
  const char *fallback[4] = {"FOOD", "JOY", "ENERGY", "HYGIENE"};
  int16_t d[4];
  pet->needRatesPerHour(d);   // engine's own rules (Pet::tick), per game hour
  for (int i = 0; i < 4; i++) {
    char key[16], line[24];
    snprintf(key, sizeof(key), "V_NEEDS_%d", i);
    int y = 84 + i * 44;
    text(T(key, fallback[i]), 82, y, 2, p.ink);
    drawBarH(82, y + 18, 230, 14, vals[i], 100, needColour(vals[i]), p.dim);
    snprintf(line, sizeof(line), "%3u  %+d/h", vals[i], d[i]);
    text(line, 318, y + 22, 1, p.ink);
  }
  char line[64];
  snprintf(line, sizeof(line), "%s: %u  %s: %u", T("S_ST_WEIGHT", "Weight"), pet->weight(), T("S_ST_MESS", "Mess"), pet->mess());
  textCentered(line, 268, 2, p.ink);
  snprintf(line, sizeof(line), "%s: %u", T("S_ST_MISTAKES", "Care mistakes"), pet->careMistakes());
  textCentered(line, 300, 2, pet->careMistakes() ? tama::kPal_bar_warn : p.ink);
  fmt(line, sizeof(line), T("S_ST_MISTAKE_NOTE", "each delays {evolve_verb} by 1 Lv"));
  textCentered(line, 326, 1, p.ink);
}

// --------------------------------------------------------------------------- growth

static void pageGrowth(const Palette &p) {
  char line[64], dur[24];
  fmtDuration(dur, sizeof(dur), (int32_t)pet->ageMinutes());
  snprintf(line, sizeof(line), "Lv %u   %s %s", pet->level(), T("S_ST_AGE", "age"), dur);
  textCentered(line, 84, 2, p.ink);
  const tama::SpeciesDef *d = pet->speciesDef();
  // evolution line: current form -> each next form (silhouette until collected)
  int cy = 190;
  if (d->evoCount == 0) {
    drawThumb(pet->species(), CX, cy, 2, pet->variant(), false);
    textCentered(T("S_ST_FINAL", "Final form"), cy + 48, 2, p.ink);
    uint32_t need = (uint32_t)tama::kTheme.farewellAgeDays * 24 * 60;
    if (pet->canFarewell()) {
      textCentered(T("S_ST_FAREWELL_READY", "Ready to say goodbye"), cy + 80, 2, tama::kPal_bar_warn);
    } else if (pet->ageMinutes() < need) {
      fmtDuration(dur, sizeof(dur), (int32_t)(need - pet->ageMinutes()));
      fmt(line, sizeof(line), T("S_ST_FAREWELL_IN", "goodbye possible in {name}"), dur);
      textCentered(line, cy + 80, 1, p.ink);
    }
    return;
  }
  int n = d->evoCount;
  drawThumb(pet->species(), n == 1 ? CX - 80 : CX - 110, cy, 2, pet->variant(), false);
  for (int i = 0; i < n && i < 3; i++) {
    int x = n == 1 ? CX + 80 : CX + 10 + i * 90;
    int y = n == 1 ? cy : cy - 30 + i * 0;
    uint16_t to = d->evos[i].to;
    bool seen = pet->isSeen(to);
    drawThumb(to, x, y, 2, false, !seen);
    snprintf(line, sizeof(line), "Lv %u", d->evos[i].level + pet->careMistakes());
    text(line, x - textW(line, 1) / 2, y + 44, 1, p.ink);
    text(seen ? speciesName(to) : "???", x - textW(seen ? speciesName(to) : "???", 1) / 2, y + 56, 1, p.ink);
  }
  text("\x1a", n == 1 ? CX - 6 : CX - 48, cy - 8, 2, p.ink);
  // A keepsake brought back out only evolves into forms not collected yet (no swap-and-evolve
  // loops), so once every branch is collected it stays as it is.
  int16_t left = pet->levelsToEvolve();
  if (pet->evolutionsExhausted()) {
    textCentered(T("S_ST_ALL_FORMS", "All forms collected"), 300, 2, p.ink);
  } else if (left > 0) {
    snprintf(line, sizeof(line), "%d %s", left, T("S_ST_LEVELS_TO_GO", "levels to go"));
    textCentered(line, 300, 2, p.ink);
  } else if (pet->canEvolve()) {
    fmt(line, sizeof(line), T("S_ST_EVO_READY", "Ready to {evolve_verb}!"));
    textCentered(line, 300, 2, tama::kPal_bar_ok);
  } else {
    textCentered(T("S_MS_BLOCK_NEEDS", "needs all 40+"), 300, 2, tama::kPal_bar_warn);
  }
}

// --------------------------------------------------------------------------- bond & streak

static void pageBond(const Palette &p) {
  char line[64];
  snprintf(line, sizeof(line), "%s %u/100", T("S_ST_BOND", "Bond"), pet->bond());
  text(line, 82, 84, 2, p.ink);
  drawBarH(82, 106, 302, 14, pet->bond(), 100, tama::kPal_accent_default, p.dim);
  snprintf(line, sizeof(line), "%s %u/20", T("S_ST_TODAY", "today"), pet->bondToday());
  text(line, 384 - textW(line, 1), 88, 1, p.ink);
  snprintf(line, sizeof(line), "%s %u  (%s %u)", T("S_ST_STREAK", "Streak"), pet->streak(), T("S_ST_BEST", "best"), pet->bestStreak());
  text(line, 82, 134, 2, p.ink);
  static const char *names[tama::kMedalCount] = {"Lv 10", "Lv 25", "Lv 50", "Favourite", "7-day streak", "Max bond", "Final form", "Fit"};
  for (int i = 0; i < tama::kMedalCount; i++) {
    char key[16];
    snprintf(key, sizeof(key), "S_MEDAL_%d", i);
    bool got = pet->medals() & (1u << i);
    int x = i % 2 ? CX + 8 : 92, y = 172 + (i / 2) * 36;
    gfx->fillCircle(x + 8, y + 7, 7, got ? tama::kPal_bar_warn : p.dim);
    text(T(key, names[i]), x + 22, y, got ? 2 : 1, got ? p.ink : p.dim);
  }
  snprintf(line, sizeof(line), "%s: %u", T("S_ST_TOTAL_MEDALS", "All-time medals"), pet->totalMedals());
  textCentered(line, 326, 1, p.ink);
}

// --------------------------------------------------------------------------- collection

static void pageCollection(const Palette &p) {
  char line[64];
  uint16_t n = tama::kTheme.speciesCount, alt = 0;
  for (uint16_t i = 0; i < n; i++) alt += pet->isVariantCollected(i);
  fmt(line, sizeof(line), T("S_ST_CHRONICLE", "{collection}"));
  text(line, 82, 82, textW(line, 2) <= 300 ? 2 : 1, p.ink);
  drawBarH(82, 102, 230, 12, pet->collectedCount(), n, tama::kPal_bar_ok, p.dim);
  snprintf(line, sizeof(line), "%u/%u", pet->collectedCount(), n);
  text(line, 320, 104, 1, p.ink);
  snprintf(line, sizeof(line), "%s: %u/%u", T("V_VARIANT", "Rare colour"), alt, n);
  text(line, 82, 124, 1, p.ink);
  fmt(line, sizeof(line), "{storage}");
  text(line, 82, 146, 2, p.ink);
  drawBarH(82, 166, 230, 12, pet->ownedCount(), pet->storageCapacity(), tama::kPal_accent_default, p.dim);
  snprintf(line, sizeof(line), "%u/%u", pet->ownedCount(), pet->storageCapacity());
  text(line, 320, 168, 1, p.ink);
  int y = 196;
  // Eggs come in the colour being hunted (docs/design.md §7): normal first,
  // then only rare colours, then the player's choice. Tier locks count that colour.
  tama::Phase ph = pet->phase();
  if (ph == tama::Phase::Complete) {
    fmt(line, sizeof(line), T("S_ST_PHASE_DONE", "{collection} complete!"));
    textCentered(line, y + 12, 2, tama::kPal_bar_ok);
    y += 52;
  }
  uint16_t have = ph == tama::Phase::Normal ? pet->collectedCount() : alt;
  for (uint8_t t = 0; t + 1 < tama::kTheme.tierCount && ph != tama::Phase::Complete; t++) {
    const tama::TierDef &td = tama::kTheme.tiers[t];
    if (!td.unlockCollected) continue;
    char tier[24];   // tier name ("mythic" -> "MYTHIC"), like the TierUnlock milestone
    snprintf(tier, sizeof(tier), "%s%s", ph == tama::Phase::Variant ? "*" : "", td.id);
    for (char *c = tier; *c; c++) if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);
    bool open = have >= td.unlockCollected;
    snprintf(line, sizeof(line), "%s %s %u/%u", open ? "\xfb" : " ", tier,
             have < td.unlockCollected ? have : td.unlockCollected, td.unlockCollected);
    text(line, 82, y, 2, open ? tama::kPal_bar_ok : p.ink);
    y += 26;
  }
  int bonus = (int)lroundf(pet->careBonus() / 28.0f * 100.0f);
  snprintf(line, sizeof(line), "%s +%d%%", T("S_ST_ODDS", "Rare egg odds"), bonus);
  textCentered(line, 318, 2, p.ink);
}

// --------------------------------------------------------------------------- egg + render

static void pageEgg(const Palette &p) {
  Milestone ms[tama::kMaxMilestones];
  uint8_t n = pet->milestones(ms, tama::kMaxMilestones);
  if (n) drawMilestone(ms[0], 120, p);
  uint8_t tier = pet->eggTier();
  if (tier + 1 < tama::kTheme.tierCount) textCentered(tama::themeTierHint(tier), 240, 2, tama::kPal_bar_warn);
}

void statusRender(uint32_t now) {
  Palette p = palette();
  fillBackground(p.bg);
  uint8_t page = router.page();
  drawBackMark(p.ink);
  if (pet->isEgg()) {
    drawTitle(T("S_ST_EGG", "HATCHING"), p.ink);
    pageEgg(p);
    return;
  }
  drawTitle(T(kPageKeys[page], kPageFallback[page]), p.ink);
  switch (page) {
    case 0: pageNext(p); break;
    case 1: pageCare(p); break;
    case 2: pageGrowth(p); break;
    case 3: pageBond(p); break;
    default: pageCollection(p); break;
  }
  drawPageDots(statusPages(), page, p.ink, p.dim);
  (void)now;
}

void statusTap(int x, int y, uint32_t now) {
  (void)x; (void)y; (void)now;
}

}  // namespace ui
