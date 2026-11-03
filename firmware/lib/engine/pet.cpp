// The pet: lifecycle, time and ticks, endings, care actions, need rates and milestones.
// Eggs + evolution: pet_collection.cpp. Storage: pet_storage.cpp. Saves: pet_save.cpp.
#include "pet.h"

#include <cstring>
#include <initializer_list>

#include "pet_format.h"

namespace tama {

using namespace detail;


Pet::Pet(const ThemeDef &theme, KvStore &kv, Rng &rng, const Config &cfg)
    : theme_(theme), kv_(kv), rng_(rng), cfg_(cfg) {}

uint8_t Pet::dropTo(uint8_t v, uint8_t d, uint8_t floor) {
  if (v <= floor) return v;
  return (v - floor > d) ? (uint8_t)(v - d) : floor;
}

// --------------------------------------------------------------------------- lifecycle

void Pet::begin(uint32_t nowMs) {
  lastTick_ = nowMs;
  if (!load()) {
    s_ = State();
    newEgg();
    save();
  }
  if (!loadStorage()) boxCount_ = 0;
}

void Pet::factoryReset(uint32_t nowMs) {
  kv_.remove(kSaveKey);
  kv_.remove(kBoxKey);
  boxCount_ = 0;
  s_ = State();
  eatUntil_ = heartUntil_ = evolveUntil_ = ceremonyUntil_ = medalUntil_ = milestoneUntil_ = 0;
  begin(nowMs);
}

void Pet::update(uint32_t nowMs) {
  if (s_.ceremony != (uint8_t)Ending::None && !timeLeft(ceremonyUntil_, nowMs)) {
    newEgg();
    save();
    lastTick_ = nowMs;
    return;
  }
  // Signed difference: correct across the millis() wrap.
  while ((int32_t)(nowMs - lastTick_) >= (int32_t)cfg_.tickMs) {
    lastTick_ += cfg_.tickMs;
    tick();
  }
  // Edge-triggered "ready" events, so the UI can chime once.
  bool evo = wantEvolveButton(), run = canRunaway();
  if (evo && !wasEvoReady_) events_ |= EV_EVO_READY;
  if (run && !wasRunReady_) events_ |= EV_RUNAWAY_READY;
  wasEvoReady_ = evo;
  wasRunReady_ = run;
  if (newMedal_ && !timeLeft(medalUntil_, nowMs)) newMedal_ = 0;
  (void)nowMs;
}

void Pet::newEgg() {
  s_.ceremony = (uint8_t)Ending::None;
  s_.neglectTicks = 0;
  s_.weight = 0;
  s_.prevSpecies = kNoSpecies;
  s_.species = kNoSpecies;
  s_.starterPick = collectedCount() == 0 && variantCollectedCount() == 0 && theme_.starterCount > 0;
  prepareEgg();
  s_.awakenedStint = false;
  s_.stintMinutes = 0;
  s_.eggTaps = 0;
  s_.food = 80; s_.joy = 80; s_.energy = 80; s_.hygiene = 100;
  s_.mess = 0;
  s_.ageMinutes = 0;
  s_.careMistakes = 0;
  s_.mistakeCooldown = 0;
  s_.sleeping = false;
  s_.evoDeclinedLv = 0;
  s_.farDeclinedAge = 0;
  s_.goodTicks = 0;
  events_ |= EV_NEW_EGG;
}

bool Pet::luckyVariant() {
  int base = (int)theme_.variantOdds;
  if (s_.lastEnd == (uint8_t)Ending::Farewell) base /= 2;
  base -= careBonus();
  if (base < 8) base = 8;
  return rng_.below((uint32_t)base) == 0;
}

// The next egg (docs/design.md §7). Normal phase: something not collected
// in normal colour, or, on a lucky roll, in rare colour. Variant phase: always a rare colour
// not collected yet. Complete: the player chooses. Each colour falls back to the other only if
// it has nothing left, so an egg is never a repeat while anything is still missing.
void Pet::prepareEgg() {
  s_.eggPick = false;
  if (s_.starterPick) {                 // first game: the player picks one of the starters
    s_.eggTarget = theme_.starters[rng_.below(theme_.starterCount)];
    s_.eggVariant = luckyVariant();
    return;
  }
  Phase ph = phase();
  if (ph == Phase::Complete) {
    s_.eggPick = true;
    s_.eggTarget = kNoSpecies;
    s_.eggVariant = false;
    return;
  }
  bool variant = ph == Phase::Variant || luckyVariant();
  uint16_t sp = rollEggSpecies(variant);
  if (sp == kNoSpecies) {
    variant = !variant;
    sp = rollEggSpecies(variant);
  }
  if (sp == kNoSpecies) {               // nothing left to hatch in either colour: let them choose
    s_.eggPick = true;
    s_.eggTarget = kNoSpecies;
    s_.eggVariant = false;
    return;
  }
  s_.eggTarget = sp;
  s_.eggVariant = variant;
}

void Pet::chooseStarter(uint8_t i) {
  if (!s_.starterPick || i >= theme_.starterCount) return;
  s_.eggTarget = theme_.starters[i];
  s_.starterPick = false;
  save();
}

bool Pet::chooseEgg(uint16_t species, bool variant) {
  if (!s_.eggPick || !isEgg() || species >= theme_.speciesCount || !isBaseForm(species)) return false;
  s_.eggTarget = species;
  s_.eggVariant = variant;
  s_.eggPick = false;
  s_.ageMinutes = 0;                    // the hatch timer starts now, not when the egg appeared
  save();
  return true;
}

void Pet::hatch(uint32_t nowMs) {
  s_.species = s_.eggTarget;
  s_.variant = s_.eggVariant;
  s_.favouriteKnown = false;
  s_.bond = 0;
  s_.bondToday = 0;
  s_.medals = 0;
  newMedal_ = 0;
  s_.nick[0] = 0;
  s_.origin = (uint8_t)Origin::Hatched;
  s_.keepsakeOf = kNoSpecies;
  s_.keptFromNick[0] = 0;
  s_.awakenedStint = false;
  s_.stintMinutes = 0;
  collect(s_.species);
  checkMedals(nowMs);   // a final form can hatch directly
  events_ |= EV_HATCH;
  save();
}

// Collected in the pet's own colour only: a rare-colour pet doesn't fill the normal entry.
void Pet::collect(uint16_t idx) {
  if (idx >= theme_.speciesCount) return;
  Phase before = phase();
  setBitIn(s_.variant ? s_.collectedVariant : s_.collected, idx);
  if (phase() != before) events_ |= EV_PHASE;
}

void Pet::debugCollect(uint16_t idx, bool variant) {
  if (idx >= theme_.speciesCount) return;
  Phase before = phase();
  setBitIn(variant ? s_.collectedVariant : s_.collected, idx);
  if (phase() != before) events_ |= EV_PHASE;
}

uint16_t Pet::collectedCount() const {
  uint16_t n = 0;
  for (uint16_t i = 0; i < theme_.speciesCount; i++) n += isCollected(i);
  return n;
}

uint16_t Pet::variantCollectedCount() const {
  uint16_t n = 0;
  for (uint16_t i = 0; i < theme_.speciesCount; i++) n += isVariantCollected(i);
  return n;
}

Phase Pet::phase() const {
  if (collectedCount() < theme_.speciesCount) return Phase::Normal;
  if (variantCollectedCount() < theme_.speciesCount) return Phase::Variant;
  return Phase::Complete;
}

// --------------------------------------------------------------------------- time

void Pet::setClock(uint32_t nowEpoch) {
  s_.lastSeenEpoch = nowEpoch;
  if (nowEpoch) save();
}

// Time spent powered off: needs drop with mercy (floor 15 awake), no care mistakes, no
// runaway and no evolution while away. Evolution waits for the player to watch it.
void Pet::syncClock(uint32_t nowEpoch) {
  uint32_t seen = s_.lastSeenEpoch;
  s_.lastSeenEpoch = nowEpoch;
  if (!nowEpoch) return;
  uint32_t mins = (seen && nowEpoch > seen) ? (nowEpoch - seen) / 60 : 0;
  if (mins < 2 || s_.ceremony != (uint8_t)Ending::None || s_.starterPick || s_.eggPick) {
    save();
    return;
  }
  if (mins > cfg_.offlineCapMin) mins = cfg_.offlineCapMin;
  for (uint32_t i = 0; i < mins; i++) {
    s_.ageMinutes++;
    if (isEgg()) {
      if (s_.ageMinutes >= cfg_.eggHatchMin) hatch(0);
      continue;
    }
    if (s_.stintMinutes < 0xFFFFFFFFu) s_.stintMinutes++;   // time away is time with you
    if (s_.sleeping) {
      s_.energy = clamp100(s_.energy + 6);
      if (s_.ageMinutes % 2 == 0) {
        s_.food = dropTo(s_.food, 1, 30);
        s_.joy = dropTo(s_.joy, 1, 35);
      }
      if (s_.ageMinutes % 3 == 0) s_.hygiene = dropTo(s_.hygiene, 1, 45);
      continue;
    }
    s_.food = dropTo(s_.food, 2, 15);
    s_.energy = dropTo(s_.energy, 1, 15);
    s_.hygiene = dropTo(s_.hygiene, 1, 15);
    s_.joy = dropTo(s_.joy, 1, 15);
  }
  if (!isEgg() && !s_.sleeping) {
    uint32_t m = s_.mess + mins / 240;
    s_.mess = m > 3 ? 3 : (uint8_t)m;
  }
  save();
}

bool Pet::resumeIfSuspended(uint32_t nowMs, uint32_t nowEpoch) {
  uint32_t seen = s_.lastSeenEpoch;
  if (!nowEpoch || !seen || nowEpoch <= seen || nowEpoch - seen < 120) return false;
  syncClock(nowEpoch);
  lastTick_ = nowMs;
  return true;
}

void Pet::tick() {
  if (s_.ceremony != (uint8_t)Ending::None) return;  // time stops during an ending
  if (s_.starterPick || s_.eggPick) return;   // TamaPoke 1.3: no hatching while choosing
  s_.ageMinutes++;

  if (isEgg()) {
    if (s_.ageMinutes >= cfg_.eggHatchMin) hatch(lastTick_);
    return;
  }
  if (s_.stintMinutes < 0xFFFFFFFFu) s_.stintMinutes++;

  if (s_.sleeping) {  // rest: energy back, needs drop ~4x slower with floors
    s_.energy = clamp100(s_.energy + 6);
    if (s_.weight > 0 && s_.ageMinutes % 3 == 0) s_.weight--;
    if (s_.ageMinutes % 2 == 0) {
      s_.food = dropTo(s_.food, 1, 30);
      s_.joy = dropTo(s_.joy, 1, 35);
    }
    if (s_.ageMinutes % 3 == 0) s_.hygiene = dropTo(s_.hygiene, 1, 45);
    checkMedals(lastTick_);
    if (++ticksSinceSave_ >= 5) pendingSave_ = true;
    return;
  }

  if (s_.ageMinutes % cfg_.minutesPerLevel == 0) events_ |= EV_LEVEL_UP;

  s_.food = clamp100(s_.food - 2);
  s_.energy = clamp100(s_.energy - 1);
  if (s_.food > 40 && s_.mess < 3 && rng_.below(100) < 15) s_.mess++;
  s_.hygiene = clamp100(s_.hygiene - 1 - 4 * s_.mess);
  if (s_.weight > 50) s_.energy = clamp100(s_.energy - 1);   // chubby = sluggish
  if (s_.weight > 0 && s_.ageMinutes % 3 == 0) s_.weight--;

  int dJoy = -1;
  if (s_.food < 30) dJoy -= 2;
  if (s_.hygiene < 30) dJoy -= 2;
  s_.joy = clamp100(s_.joy + dJoy);

  // care mistake: any need <= 10, counted once per hour
  if (s_.mistakeCooldown > 0) s_.mistakeCooldown--;
  if (lowestNeed() <= 10 && s_.mistakeCooldown == 0) {
    // Saturate: a uint8_t wrapped to 0 after ~10 days of neglect (one per hour), wiping the
    // record and making evolution instantly available (found by the playthrough test).
    if (s_.careMistakes < 255) s_.careMistakes++;
    s_.mistakeCooldown = 60;
    if (s_.bond > 1) s_.bond--;   // TamaPoke 1.4: bigger penalties made bond unrecoverable
  }
  if (lowestNeed() >= 40) {
    if (s_.goodTicks < 0xFFFF) s_.goodTicks++;
  } else {
    s_.goodTicks = 0;
  }

  checkMedals(lastTick_);

  // total neglect for an hour -> ready to run away (the player triggers it, it never auto-fires)
  if (s_.food == 0 && s_.joy == 0 && s_.energy == 0 && s_.hygiene == 0) {
    if (s_.neglectTicks < cfg_.runawayTicks) s_.neglectTicks++;
  } else {
    s_.neglectTicks = 0;   // a single act of care cancels it
  }

  if (++ticksSinceSave_ >= 5) pendingSave_ = true;
}

uint16_t Pet::level() const {
  uint32_t lv = 1 + s_.ageMinutes / cfg_.minutesPerLevel;
  return lv > 999 ? 999 : (uint16_t)lv;   // TamaPoke 1.8: a uint8_t wrapped after ~10 days
}

uint8_t Pet::lowestNeed() const {
  uint8_t a = s_.food < s_.joy ? s_.food : s_.joy;
  uint8_t b = s_.energy < s_.hygiene ? s_.energy : s_.hygiene;
  return a < b ? a : b;
}

Mood Pet::mood(uint32_t nowMs) const {
  if (s_.sleeping) return Mood::Sleeping;
  if (eating(nowMs)) return Mood::Eating;
  if (lowestNeed() < 25) return Mood::Sad;
  return Mood::Happy;
}

float Pet::evolveProgress(uint32_t now) const {
  return 1.0f - (float)timeLeft(evolveUntil_, now) / (float)cfg_.evolveAnimMs;
}

float Pet::ceremonyProgress(uint32_t now) const {
  if (s_.ceremony == (uint8_t)Ending::None) return 0.0f;
  return 1.0f - (float)timeLeft(ceremonyUntil_, now) / (float)cfg_.ceremonyMs;
}

// --------------------------------------------------------------------------- endings

bool Pet::canFarewell() const {
  if (isEgg() || s_.sleeping || s_.ceremony != (uint8_t)Ending::None) return false;
  return theme_.species[s_.species].evoCount == 0 &&
         s_.ageMinutes >= (uint32_t)theme_.farewellAgeDays * 24 * 60;
}

bool Pet::canRunaway() const {
  return !isEgg() && !s_.sleeping && s_.ceremony == (uint8_t)Ending::None &&
         s_.neglectTicks >= cfg_.runawayTicks;
}

void Pet::startEnding(Ending e, uint32_t nowMs) {
  if (isEgg() || s_.ceremony != (uint8_t)Ending::None) return;
  s_.lastEnd = (uint8_t)e;
  s_.ceremony = (uint8_t)e;
  ceremonyUntil_ = deadlineIn(nowMs, cfg_.ceremonyMs);
  if (e != Ending::Runaway) heartUntil_ = ceremonyUntil_;
  events_ |= EV_ENDING;
  save();
}

// --------------------------------------------------------------------------- care

// Every act of care. Clearing the neglect counter here (not in tick()) is what makes "a
// single act of care cancels the runaway" true: with all needs at 0, petting's +5 joy is
// eaten by the next tick's -5, so a tick-based check never saw the care (TamaPoke had this).
void Pet::registerCare(uint32_t nowMs) {
  if (isEgg() || s_.ceremony != (uint8_t)Ending::None) return;
  s_.neglectTicks = 0;
  uint32_t d = today();
  if (d == 0 || d == s_.lastCareDay) return;
  if (s_.lastCareDay == 0 || d == s_.lastCareDay + 1) {
    s_.streak++;
  } else {
    s_.streak = 1;
    s_.lastMilestone = 0;
  }
  s_.lastCareDay = d;
  s_.bondToday = 0;
  if (s_.streak > s_.bestStreak) s_.bestStreak = s_.streak;
  s_.bond = clamp100(s_.bond + 4);
  uint16_t ms = s_.streak >= 100 ? 100 : s_.streak >= 30 ? 30 : s_.streak >= 7 ? 7 : s_.streak >= 3 ? 3 : 0;
  if (ms > s_.lastMilestone) {
    s_.lastMilestone = ms;
    milestoneUntil_ = deadlineIn(nowMs, 4500);
    events_ |= EV_MILESTONE;
  }
  checkMedals(nowMs);
}

void Pet::addBond(uint8_t amt) {
  if (s_.bondToday >= 20) return;   // daily cap: bond can't be farmed
  s_.bond = clamp100(s_.bond + amt);
  s_.bondToday = (uint8_t)(s_.bondToday + amt);
}

void Pet::checkMedals(uint32_t nowMs) {
  if (isEgg()) return;
  uint16_t before = s_.medals;
  uint16_t lv = level();
  if (lv >= 10) s_.medals |= MED_LV10;
  if (lv >= 25) s_.medals |= MED_LV25;
  if (lv >= 50) s_.medals |= MED_LV50;
  if (s_.favouriteKnown) s_.medals |= MED_FAVOURITE;
  if (s_.streak >= 7) s_.medals |= MED_STREAK7;
  if (s_.bond >= 100) s_.medals |= MED_BOND;
  if (theme_.species[s_.species].evoCount == 0) s_.medals |= MED_FINAL;
  if (s_.weight == 0 && lv >= 5 && s_.careMistakes == 0) s_.medals |= MED_FIT;
  uint16_t gained = s_.medals & (uint16_t)~before;
  if (gained) {
    for (uint16_t m = gained; m; m &= (uint16_t)(m - 1)) s_.totalMedals++;
    newMedal_ = gained;
    medalUntil_ = deadlineIn(nowMs, 4000);
    events_ |= EV_MEDAL;
    pendingSave_ = true;
  }
}

void Pet::feed(uint8_t food, uint32_t nowMs) {
  if (isEgg() || s_.sleeping || s_.ceremony != (uint8_t)Ending::None || food > 2) return;
  if (theme_.species[s_.species].favouriteFood == food) {
    s_.food = clamp100(s_.food + 35);
    s_.joy = clamp100(s_.joy + 10);
    heartUntil_ = deadlineIn(nowMs, cfg_.heartMs);
    s_.favouriteKnown = true;
    events_ |= EV_HEART;
    addBond(2);
  } else {
    s_.food = clamp100(s_.food + 25);
  }
  eatUntil_ = deadlineIn(nowMs, cfg_.eatAnimMs);
  registerCare(nowMs);
  checkMedals(nowMs);
  save();
}

void Pet::feedTreat(uint32_t nowMs) {
  if (isEgg() || s_.sleeping || s_.ceremony != (uint8_t)Ending::None) return;
  s_.food = clamp100(s_.food + 10);
  s_.joy = clamp100(s_.joy + 12);
  s_.weight = clamp100(s_.weight + 12);   // treats fatten
  eatUntil_ = deadlineIn(nowMs, cfg_.eatAnimMs);
  registerCare(nowMs);
  save();
}

void Pet::playResult(uint16_t score, uint32_t nowMs) {
  if (isEgg() || s_.ceremony != (uint8_t)Ending::None) return;
  s_.joy = clamp100(s_.joy + 5 + (score > 15 ? 30 : score * 2));
  uint32_t tire = 10 + score / 2;
  s_.energy = dropTo(s_.energy, (uint8_t)(tire > 95 ? 95 : tire), 5);
  s_.food = dropTo(s_.food, 5, 5);
  int burn = (int)s_.weight - (int)score * 2;   // exercise burns weight
  s_.weight = burn > 0 ? (uint8_t)burn : 0;
  if (score >= 5) {
    heartUntil_ = deadlineIn(nowMs, cfg_.heartMs);
    events_ |= EV_HEART;
  }
  if (score > s_.playHi) s_.playHi = score;
  addBond(2);
  registerCare(nowMs);
  save();
}

void Pet::toggleSleep() {
  if (isEgg() || s_.ceremony != (uint8_t)Ending::None) return;
  s_.sleeping = !s_.sleeping;
  save();
}

void Pet::clean(uint32_t nowMs) {
  if (isEgg() || s_.ceremony != (uint8_t)Ending::None) return;
  s_.mess = 0;
  s_.hygiene = 100;
  addBond(1);
  registerCare(nowMs);
  save();
}

void Pet::caress(uint32_t nowMs) {
  if (isEgg() || s_.sleeping || s_.ceremony != (uint8_t)Ending::None) return;
  s_.joy = clamp100(s_.joy + 5);
  heartUntil_ = deadlineIn(nowMs, cfg_.heartMs);
  events_ |= EV_HEART;
  addBond(1);
  registerCare(nowMs);
  pendingSave_ = true;   // petting is frequent: let the periodic save pick it up
}

void Pet::rename(const char *nick) {
  strncpy(s_.nick, nick ? nick : "", kNickLen - 1);
  s_.nick[kNickLen - 1] = 0;
  save();
}

// --------------------------------------------------------------------------- rates

void Pet::needRatesPerHour(int16_t out[4]) const {
  if (isEgg()) { out[0] = out[1] = out[2] = out[3] = 0; return; }
  if (s_.sleeping) {           // tick(): energy +6/min, food & joy -1 per 2 min, hygiene -1 per 3
    out[0] = -30; out[1] = -30; out[2] = 360; out[3] = -20;
    return;
  }
  out[0] = -2 * 60;
  int joy = -1;
  if (s_.food < 30) joy -= 2;
  if (s_.hygiene < 30) joy -= 2;
  out[1] = (int16_t)(joy * 60);
  out[2] = (int16_t)((s_.weight > 50 ? -2 : -1) * 60);
  out[3] = (int16_t)((-1 - 4 * (int)s_.mess) * 60);
}

// --------------------------------------------------------------------------- milestones

uint8_t Pet::milestones(Milestone *out, uint8_t max) const {
  Milestone m[kMaxMilestones];
  uint8_t n = 0;
  auto add = [&](const Milestone &x) { if (n < kMaxMilestones && x.target) m[n++] = x; };
  const uint32_t mpl = cfg_.minutesPerLevel;
  const uint32_t age = s_.ageMinutes;

  if (isEgg()) {
    Milestone x;
    x.kind = MilestoneKind::Hatch;
    x.unit = MilestoneUnit::Taps;
    x.current = s_.eggTaps;
    x.target = 3;
    x.etaMinutes = (s_.starterPick || s_.eggPick) ? -1 : (age >= cfg_.eggHatchMin ? 0 : (int32_t)(cfg_.eggHatchMin - age));
    add(x);
  } else {
    if (level() < 999) {
      Milestone x;
      x.kind = MilestoneKind::NextLevel;
      x.unit = MilestoneUnit::Minutes;
      x.current = age % mpl;
      x.target = mpl;
      x.etaMinutes = (int32_t)(mpl - x.current);
      add(x);
    }
    const SpeciesDef &d = theme_.species[s_.species];
    if (d.evoCount) {
      uint16_t minLv = 0xFFFF, open = kNoSpecies;
      uint8_t openCount = 0;
      uint8_t usable = 0;
      uint16_t only = kNoSpecies;
      for (uint8_t e = 0; e < d.evoCount; e++) {
        if (!evoTargetOpen(d.evos[e].to)) continue;
        usable++;
        only = d.evos[e].to;
        uint16_t lv = (uint16_t)(d.evos[e].level + s_.careMistakes);
        if (lv < minLv) minLv = lv;
        if (!collectedInColour(d.evos[e].to)) { open = d.evos[e].to; openCount++; }
      }
      if (usable) {
        Milestone x;
        x.kind = MilestoneKind::Evolution;
        x.unit = MilestoneUnit::Levels;
        x.current = level();
        x.target = minLv;
        uint32_t needAge = (uint32_t)(minLv - 1) * mpl;
        x.etaMinutes = age >= needAge ? 0 : (int32_t)(needAge - age);
        if (lowestNeed() < 40) x.blocked |= MS_BLOCK_NEEDS;
        if (s_.sleeping) x.blocked |= MS_BLOCK_ASLEEP;
        x.ref = usable == 1 ? only : (openCount == 1 ? open : kNoSpecies);
        add(x);
      }   // no usable target: a keepsake with every form collected (evolutionsExhausted())
    } else {
      uint32_t need = (uint32_t)theme_.farewellAgeDays * 24 * 60;
      if (age < need) {
        Milestone x;
        x.kind = MilestoneKind::Farewell;
        x.unit = MilestoneUnit::Minutes;
        x.current = age;
        x.target = need;
        x.etaMinutes = (int32_t)(need - age);
        add(x);
      }
    }
    static const struct { uint16_t lv, bit; } kLvMedals[] = {{10, MED_LV10}, {25, MED_LV25}, {50, MED_LV50}};
    for (const auto &lm : kLvMedals) {
      if ((s_.medals & lm.bit) || level() >= lm.lv) continue;
      Milestone x;
      x.kind = MilestoneKind::LevelMedal;
      x.unit = MilestoneUnit::Levels;
      x.current = level();
      x.target = lm.lv;
      x.etaMinutes = (int32_t)((uint32_t)(lm.lv - 1) * mpl - age);
      x.ref = lm.bit;
      add(x);
      break;   // only the nearest one
    }
    if (s_.bond < 100) {
      Milestone x;
      x.kind = MilestoneKind::Bond;
      x.unit = MilestoneUnit::Points;
      x.current = s_.bond;
      x.target = 100;
      add(x);
    }
    for (uint16_t t : {3, 7, 30, 100}) {
      if (s_.streak >= t) continue;
      Milestone x;
      x.kind = MilestoneKind::Streak;
      x.unit = MilestoneUnit::Days;
      x.current = s_.streak;
      x.target = t;
      x.etaMinutes = (int32_t)((t - s_.streak) * 24 * 60);   // assuming care every day
      x.ref = t;
      add(x);
      break;
    }
    // tier locks count the colour the eggs come in now (rare colour once normal is complete)
    Phase ph = phase();
    uint16_t have = ph == Phase::Normal ? collectedCount() : variantCollectedCount();
    for (uint8_t t = 0; t < theme_.tierCount && ph != Phase::Complete; t++) {
      const TierDef &td = theme_.tiers[t];
      if (td.catchAll || td.unlockCollected <= have) continue;
      Milestone x;
      x.kind = MilestoneKind::TierUnlock;
      x.unit = MilestoneUnit::Count;
      x.current = have;
      x.target = td.unlockCollected;
      x.ref = t;
      add(x);
    }
    uint16_t cap = storageCapacity(), owned = ownedCount();
    for (uint16_t pct : {25, 50, 75, 100}) {
      uint16_t tgt = (uint16_t)((cap * pct + 99) / 100);
      if (!tgt || owned >= tgt) continue;
      Milestone x;
      x.kind = MilestoneKind::Collection;
      x.unit = MilestoneUnit::Count;
      x.current = owned;
      x.target = tgt;
      add(x);
      break;
    }
  }

  // nearest first: known ETAs ascending, then the open-ended ones by how far along they are
  auto before = [](const Milestone &a, const Milestone &b) {
    bool ka = a.etaMinutes >= 0, kb = b.etaMinutes >= 0;
    if (ka != kb) return ka;
    if (ka) return a.etaMinutes < b.etaMinutes;
    return (uint64_t)a.current * b.target > (uint64_t)b.current * a.target;
  };
  for (uint8_t i = 1; i < n; i++) {
    Milestone x = m[i];
    uint8_t j = i;
    while (j > 0 && before(x, m[j - 1])) { m[j] = m[j - 1]; j--; }
    m[j] = x;
  }
  uint8_t k = n < max ? n : max;
  for (uint8_t i = 0; i < k; i++) out[i] = m[i];
  return k;
}

}  // namespace tama
