// Eggs (which species and colour an egg rolls, never a repeat) and evolution.
#include "pet.h"

#include <cstring>
#include <initializer_list>

#include "pet_format.h"

namespace tama {

using namespace detail;

// --------------------------------------------------------------------------- eggs

int Pet::careBonus() const {
  int st = s_.streak > 30 ? 30 : s_.streak;
  return st / 3 + s_.bond / 25;   // 0..14
}

int16_t Pet::indexOfKey(uint16_t key) const {
  for (uint16_t i = 0; i < theme_.speciesCount; i++)
    if (theme_.species[i].key == key) return (int16_t)i;
  return -1;
}

bool Pet::owns(uint16_t idx, bool variant) const {
  if (!isEgg() && s_.species == idx && s_.variant == variant) return true;
  return findStored(idx, variant) >= 0;
}

// True if some form in idx's line (idx included) is still missing in this colour and no
// owned creature can grow into it: an owned creature can still evolve into everything below
// it (hatched pets always can; keepsakes into whatever isn't collected yet).
bool Pet::lineNeedsEgg(uint16_t idx, bool variant, int depth) const {
  if (idx >= theme_.speciesCount || depth > 8) return false;
  if (owns(idx, variant)) return false;
  if (!collectedIn(idx, variant)) return true;
  const SpeciesDef &d = theme_.species[idx];
  for (uint8_t e = 0; e < d.evoCount; e++)
    if (lineNeedsEgg(d.evos[e].to, variant, depth + 1)) return true;
  return false;
}

// Never an egg of something already collected in that colour. The one exception keeps the
// roster finishable: a base form that left (release, runaway, released from storage) before
// its line was complete, when nothing you still own can evolve into the missing forms.
bool Pet::eggEligible(uint16_t idx, bool variant) const {
  if (!isBaseForm(idx)) return false;
  if (!collectedIn(idx, variant)) return true;
  return lineNeedsEgg(idx, variant);
}

// Tiers are rolled rarest first; a tier is locked until enough species are collected (in the
// egg's colour); a farewell blesses (blessedOdds), a runaway curses (straight to the
// catch-all tier). Streak and bond scale every tier's odds by up to 1.5x.
uint8_t Pet::rollTier(bool variant) {
  uint8_t last = theme_.tierCount ? theme_.tierCount - 1 : 0;
  if (s_.lastEnd == (uint8_t)Ending::Runaway) return last;
  bool blessed = s_.lastEnd == (uint8_t)Ending::Farewell;
  float scale = 1.0f + careBonus() / 28.0f;
  uint16_t have = variant ? variantCollectedCount() : collectedCount();
  float r = rng_.percent(), acc = 0;
  for (uint8_t t = 0; t < theme_.tierCount; t++) {
    const TierDef &td = theme_.tiers[t];
    if (td.catchAll) return t;
    if (have < td.unlockCollected) continue;
    acc += (blessed ? td.blessedOdds : td.odds) * scale;
    if (r < acc) return t;
  }
  return last;
}

uint16_t Pet::rollEggSpecies(bool variant) {
  if (collectedCount() == 0 && variantCollectedCount() == 0 && theme_.starterCount)
    return theme_.starters[rng_.below(theme_.starterCount)];   // first game: a starter (player picks)
  const uint8_t count = theme_.tierCount;
  if (!count) return kNoSpecies;
  uint8_t tier = rollTier(variant);
  uint16_t have = variant ? variantCollectedCount() : collectedCount();
  // The rolled tier has nothing left: the nearest one that has, more common first, then
  // rarer. Unlocked tiers first; a lock only delays a tier, it never blocks the last species.
  uint8_t order[256];
  uint8_t no = 0;
  for (int t = tier; t < count; t++) order[no++] = (uint8_t)t;
  for (int t = (int)tier - 1; t >= 0; t--) order[no++] = (uint8_t)t;
  for (int pass = 0; pass < 2; pass++) {
    for (uint8_t k = 0; k < no; k++) {
      const TierDef &td = theme_.tiers[order[k]];
      if (pass == 0 && !td.catchAll && have < td.unlockCollected) continue;
      uint16_t cand[kMaxSpecies];
      uint16_t n = 0;
      for (uint16_t i = 0; i < theme_.speciesCount; i++)
        if (theme_.species[i].tier == order[k] && eggEligible(i, variant)) cand[n++] = i;
      if (n) return cand[rng_.below(n)];
    }
  }
  return kNoSpecies;
}

uint8_t Pet::eggTier() const {
  return s_.eggTarget < theme_.speciesCount ? theme_.species[s_.eggTarget].tier : 0;
}

void Pet::eggTap(uint32_t nowMs) {
  if (!isEgg() || s_.starterPick || s_.eggPick) return;
  if (++s_.eggTaps >= 3) hatch(nowMs);
  else save();
}

// --------------------------------------------------------------------------- evolution

// Collected in the pet's own colour: normal and rare-colour entries are separate collection goals.
bool Pet::collectedInColour(uint16_t idx) const {
  return s_.variant ? isVariantCollected(idx) : isCollected(idx);
}

// A keepsake may only evolve into a form not collected yet in its own colour. Without this,
// evolve -> switch the keepsake back in -> evolve again looped forever, replaying evolutions,
// medals and keepsakes for free. Hatched pets can always evolve (raising a new one is the cost).
bool Pet::evoTargetOpen(uint16_t to) const {
  return s_.origin != (uint8_t)Origin::Keepsake || !collectedInColour(to);
}

bool Pet::evolutionsExhausted() const {
  if (isEgg() || s_.origin != (uint8_t)Origin::Keepsake) return false;
  const SpeciesDef &d = theme_.species[s_.species];
  if (!d.evoCount) return false;
  for (uint8_t e = 0; e < d.evoCount; e++)
    if (evoTargetOpen(d.evos[e].to)) return false;
  return true;
}

bool Pet::canEvolve() const {
  if (isEgg() || s_.sleeping || s_.ceremony != (uint8_t)Ending::None) return false;
  if (lowestNeed() < 40) return false;
  const SpeciesDef &d = theme_.species[s_.species];
  for (uint8_t e = 0; e < d.evoCount; e++)
    if (evoTargetOpen(d.evos[e].to) && level() >= (uint16_t)d.evos[e].level + s_.careMistakes) return true;
  return false;
}

int16_t Pet::levelsToEvolve() const {
  if (isEgg()) return -1;
  const SpeciesDef &d = theme_.species[s_.species];
  int best = 0x7FFF;
  for (uint8_t e = 0; e < d.evoCount; e++) {
    if (!evoTargetOpen(d.evos[e].to)) continue;
    int need = (int)d.evos[e].level + s_.careMistakes - (int)level();
    if (need < best) best = need;
  }
  if (best == 0x7FFF) return -1;   // final form, or a keepsake with every form collected
  return (int16_t)(best < 0 ? 0 : best);
}

void Pet::evolve(uint32_t nowMs) {
  if (!canEvolve()) return;
  const SpeciesDef &d = theme_.species[s_.species];
  uint16_t reached[16], fresh[16];
  uint8_t nr = 0, nf = 0;
  for (uint8_t e = 0; e < d.evoCount && nr < 16; e++) {
    if (!evoTargetOpen(d.evos[e].to) || level() < (uint16_t)d.evos[e].level + s_.careMistakes) continue;
    reached[nr++] = d.evos[e].to;
    if (!collectedInColour(d.evos[e].to)) fresh[nf++] = d.evos[e].to;
  }
  if (!nr) return;
  // Branches prefer whichever form isn't collected yet (TamaPoke's Eevee rule, generalised).
  uint16_t next = nf ? fresh[rng_.below(nf)] : reached[rng_.below(nr)];

  // Keepsake: the previous form stays in storage, one per species x colour. It keeps the age
  // it evolved at (so it can take the other branch when switched back in) but starts fresh:
  // full needs, no bond, medals, mistakes or nickname.
  if (findStored(s_.species, s_.variant) < 0) {
    Specimen k;
    k.species = s_.species;
    k.variant = s_.variant;
    k.origin = Origin::Keepsake;
    k.keepsakeOf = next;
    std::memcpy(k.keptFromNick, s_.nick, kNickLen);
    k.ageMinutes = s_.ageMinutes;
    k.favouriteKnown = s_.favouriteKnown;   // the favourite food is a fact about the species
    if (insertStored(k)) {
      events_ |= EV_KEEPSAKE;
      saveStorage();
    } else {
      events_ |= EV_STORAGE_FULL;
    }
  }
  s_.prevSpecies = s_.species;
  s_.species = next;
  s_.awakenedStint = true;   // earns "store & start a new egg" (canStoreActive)
  collect(next);
  evolveUntil_ = deadlineIn(nowMs, cfg_.evolveAnimMs);
  events_ |= EV_EVOLVE;
  checkMedals(nowMs);
  save();
}

}  // namespace tama
