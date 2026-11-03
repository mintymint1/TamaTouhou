// Collection progression (docs/design.md §7) against the real theme:
// eggs never repeat what's collected, the rare-colour phase, the free choice at the end, and
// the store-&-new-egg gate that makes raising (not switching) the way to earn eggs.
#include "framework.h"
#include "kv_mem.h"
#include "pet.h"
#include "theme_data.h"

using namespace tama;

namespace {

uint16_t sp(const char *id) {
  for (uint16_t i = 0; i < kTheme.speciesCount; i++)
    if (std::strcmp(kTheme.species[i].id, id) == 0) return i;
  std::printf("    (no species %s in theme)\n", id);
  failures()++;
  return 0;
}

bool isBase(uint16_t i) { return kTheme.species[i].tier != kTierEvo; }

uint8_t starterIndex(const char *id) {
  for (uint8_t i = 0; i < kTheme.starterCount; i++)
    if (kTheme.starters[i] == sp(id)) return i;
  return 0;
}

// a species and everything it can evolve into
void lineOf(uint16_t idx, bool *out) {
  out[idx] = true;
  const SpeciesDef &d = kTheme.species[idx];
  for (uint8_t e = 0; e < d.evoCount; e++) lineOf(d.evos[e].to, out);
}

constexpr uint32_t MIN = 60000;

struct World {
  MemKv kv;
  Rng rng{4242};
  Config cfg;
  uint32_t now = 1000;
  Pet pet{kTheme, kv, rng, cfg};

  void minutes(uint32_t m) {
    for (uint32_t i = 0; i < m; i++) {
      now += MIN;
      pet.update(now);
    }
  }
  void tap3() { pet.eggTap(now); pet.eggTap(now); pet.eggTap(now); }
  // first run with the given starter, hatched, in normal colour
  void start(const char *starter = "kitten") {
    pet.begin(now);
    pet.chooseStarter(starterIndex(starter));
    tap3();
    if (pet.variant()) pet.debugToggleVariant();
  }
  void collectAll(bool variant) {
    for (uint16_t i = 0; i < kTheme.speciesCount; i++) pet.debugCollect(i, variant);
  }
  // a fresh egg through "store & start a new egg" (the active pet counts as raised)
  void nextEgg() {
    pet.debugMarkAwakened();
    StoreError e = pet.storeActiveAndNewEgg(now, true);
    CHECK(e == StoreError::Ok);
  }
  void endCeremony() {
    now += 11000;
    pet.update(now);
  }
};

}  // namespace

// --------------------------------------------------------------------------- no repeats

TEST(eggs_never_repeat_a_collected_line) {
  World w;
  w.start("kitten");
  bool done[kMaxSpecies] = {false};   // every other base form with its whole line collected
  bool odd = false;
  for (uint16_t i = 0; i < kTheme.speciesCount; i++) {
    if (!isBase(i)) continue;
    if ((odd = !odd)) lineOf(i, done);
  }
  for (uint16_t i = 0; i < kTheme.speciesCount; i++)
    if (done[i]) w.pet.debugCollect(i, false);
  for (int k = 0; k < 5000; k++) {
    uint16_t s = w.pet.rollEggSpecies(false);
    CHECK(s != kNoSpecies);
    if (s == kNoSpecies) break;
    CHECK(isBase(s));
    CHECK(!done[s]);
    CHECK(s != sp("kitten"));          // the active pet: owned, its own line still open
  }
}

TEST(a_lost_base_form_returns_only_while_its_line_is_unfinished) {
  World w;
  w.start("kitten");
  auto rolls = [&](uint16_t target) {
    int n = 0;
    for (int k = 0; k < 6000; k++) n += w.pet.rollEggSpecies(false) == target;
    return n;
  };
  uint16_t kitten = sp("kitten");
  CHECK_EQ(rolls(kitten), 0);          // owned: it can still become Chen and Orin itself
  w.pet.release(w.now);                // let it go before it evolved
  w.endCeremony();
  CHECK(w.pet.isEgg());
  CHECK(w.pet.isCollected(kitten));
  CHECK(rolls(kitten) > 0);            // second chance: Chen and Orin need a kitten
  w.pet.debugCollect(sp("chen"), false);
  w.pet.debugCollect(sp("orin"), false);
  CHECK_EQ(rolls(kitten), 0);          // line complete: never again
}

TEST(an_owned_middle_stage_finishes_the_line_without_a_repeat) {
  World w;
  w.start("kitten");
  w.pet.debugSetSpecies(sp("reimu"));  // the active pet is something unrelated
  w.pet.debugCollect(sp("ice_fairy"), false);
  w.pet.debugCollect(sp("cirno"), false);
  Specimen cirno;
  cirno.species = sp("cirno");
  CHECK(w.pet.debugAddStored(cirno));  // Cirno can still become Tanned Cirno
  for (int k = 0; k < 6000; k++) CHECK(w.pet.rollEggSpecies(false) != sp("ice_fairy"));
  CHECK(w.pet.releaseStored((uint8_t)w.pet.findStored(sp("cirno"), false)));
  int n = 0;
  for (int k = 0; k < 6000; k++) n += w.pet.rollEggSpecies(false) == sp("ice_fairy");
  CHECK(n > 0);                        // nothing owned can reach Tanned Cirno any more
}

TEST(an_empty_tier_falls_to_the_nearest_tier_with_something_left) {
  World w;
  w.start("kitten");
  uint8_t common = kTheme.tierCount - 1;
  for (uint16_t i = 0; i < kTheme.speciesCount; i++)
    if (kTheme.species[i].tier == common) {
      bool line[kMaxSpecies] = {false};
      lineOf(i, line);
      for (uint16_t j = 0; j < kTheme.speciesCount; j++)
        if (line[j]) w.pet.debugCollect(j, false);
    }
  for (int k = 0; k < 4000; k++) {
    uint16_t s = w.pet.rollEggSpecies(false);
    CHECK(s != kNoSpecies);
    if (s == kNoSpecies) break;
    CHECK(kTheme.species[s].tier != common);
  }
}

TEST(nothing_left_in_a_colour_rolls_no_species) {
  World w;
  w.start("kitten");
  w.collectAll(false);
  CHECK_EQ(w.pet.rollEggSpecies(false), kNoSpecies);
  CHECK(w.pet.rollEggSpecies(true) != kNoSpecies);
}

// --------------------------------------------------------------------------- colours + phases

TEST(a_rare_colour_fills_only_the_rare_colour_entry) {
  World w;
  w.start("kitten");
  w.pet.debugToggleVariant();          // the active pet is now the rare colour
  w.pet.debugSetSpecies(sp("reimu"));
  CHECK(w.pet.isVariantCollected(sp("reimu")));
  CHECK(!w.pet.isCollected(sp("reimu")));
  CHECK(w.pet.isSeen(sp("reimu")));
}

TEST(phases_follow_the_two_collections) {
  World w;
  w.start("kitten");
  CHECK(w.pet.phase() == Phase::Normal);
  w.pet.takeEvents();
  w.collectAll(false);
  CHECK(w.pet.phase() == Phase::Variant);
  CHECK(w.pet.takeEvents() & EV_PHASE);
  w.collectAll(true);
  CHECK(w.pet.phase() == Phase::Complete);
  CHECK(w.pet.takeEvents() & EV_PHASE);
}

TEST(after_the_normal_chronicle_every_egg_is_a_new_rare_colour) {
  World w;
  w.start("kitten");
  w.collectAll(false);
  bool had[kMaxSpecies] = {false};
  for (int k = 0; k < 12; k++) {
    w.nextEgg();
    w.tap3();
    CHECK(!w.pet.isEgg());
    CHECK(w.pet.variant());
    CHECK(isBase(w.pet.species()));
    CHECK(!had[w.pet.species()]);      // never the same rare colour twice
    had[w.pet.species()] = true;
  }
}

TEST(the_last_rare_colour_comes_then_the_player_chooses) {
  World w;
  w.start("kitten");
  w.collectAll(false);
  for (uint16_t i = 0; i < kTheme.speciesCount; i++)
    if (i != sp("reimu")) w.pet.debugCollect(i, true);
  w.nextEgg();                         // only one thing left anywhere: a rare-colour Reimu
  w.tap3();
  CHECK_EQ(w.pet.species(), sp("reimu"));
  CHECK(w.pet.variant());
  CHECK(w.pet.phase() == Phase::Complete);
  w.nextEgg();
  CHECK(w.pet.isEgg());
  CHECK(w.pet.awaitingEggChoice());
  w.minutes(30);                       // no auto-hatch while choosing
  w.tap3();
  CHECK(w.pet.isEgg());
  CHECK(!w.pet.chooseEgg(sp("chen"), false));   // eggs hatch base forms only
  CHECK(w.pet.chooseEgg(sp("kitten"), true));
  CHECK(!w.pet.awaitingEggChoice());
  w.tap3();
  CHECK_EQ(w.pet.species(), sp("kitten"));
  CHECK(w.pet.variant());
}

// --------------------------------------------------------------------------- store gate

TEST(a_new_egg_is_earned_by_evolving) {
  World w;
  w.start("kitten");
  w.pet.debugSetLevel(20);
  CHECK(w.pet.canStoreActive() == StoreError::NotAwakened);   // Lv 20 alone isn't enough
  CHECK_EQ(w.pet.storeReadyInMinutes(), 0u);
  w.pet.debugSetNeeds(100, 100, 100, 100);
  w.pet.evolve(w.now);
  CHECK(w.pet.awakenedThisStint());
  CHECK(w.pet.canStoreActive() == StoreError::Ok);
}

TEST(nothing_left_to_evolve_needs_nine_levels_with_you) {
  World w;
  w.start("kitten");
  w.pet.debugSetSpecies(sp("reimu"));  // single stage
  CHECK(w.pet.canStoreActive() == StoreError::TooYoung);
  CHECK_EQ(w.pet.storeReadyInMinutes(), 9u * 60);
  w.pet.advanceMinutes(9 * 60 - 1);
  CHECK(w.pet.canStoreActive() == StoreError::TooYoung);
  CHECK_EQ(w.pet.storeReadyInMinutes(), 1u);
  w.pet.advanceMinutes(1);
  CHECK(w.pet.canStoreActive() == StoreError::Ok);
}

TEST(switching_a_stored_pet_in_never_gives_a_free_egg) {
  World w;
  w.start("kitten");
  w.pet.debugSetNeeds(100, 100, 100, 100);
  w.pet.debugSetLevel(16);
  w.pet.evolve(w.now);                 // kitten -> Chen/Orin, kitten kept in storage
  uint16_t evolved = w.pet.species();
  CHECK(w.pet.storeActiveAndNewEgg(w.now) == StoreError::Ok);
  w.tap3();                            // a new pet hatches
  int16_t slot = w.pet.findStored(evolved, false);
  CHECK(slot >= 0);
  CHECK(w.pet.switchWith((uint8_t)slot, w.now) == StoreError::Ok);
  CHECK_EQ(w.pet.species(), evolved);
  CHECK(w.pet.canStoreActive() == StoreError::TooYoung);       // raised elsewhere: time first
  int16_t kittenSlot = w.pet.findStored(sp("kitten"), false);
  CHECK(kittenSlot >= 0);
  CHECK(w.pet.switchWith((uint8_t)kittenSlot, w.now) == StoreError::Ok);
  CHECK(w.pet.canStoreActive() == StoreError::NotAwakened);    // the other branch is still open
  w.pet.debugSetNeeds(100, 100, 100, 100);
  w.pet.evolve(w.now);
  CHECK(w.pet.species() != evolved);
  CHECK(w.pet.canStoreActive() == StoreError::Ok);
}

// --------------------------------------------------------------------------- whole game
// A bot plays the intended loop from the first egg to 96/96 in real game minutes: raise the
// pet until it evolves, bring keepsakes back out for branches still missing, send finished
// pets to storage for a new egg, hatch it. It only acts when it checks in (every
// `careEvery` minutes while awake), where it also feeds/cleans/plays (needs to 100).

namespace {

struct Playthrough {
  uint32_t eggs = 1, repeats = 0, normalDoneMin = 0, totalMin = 0, maxMistakes = 0;
  bool complete = false;
};

Playthrough playthrough(uint32_t careEvery, bool sleepsAtNight) {
  World w;
  w.start("kitten");
  Playthrough r;
  auto openBranch = [&](const Specimen &s) {   // a stored creature that can still evolve
    const SpeciesDef &d = kTheme.species[s.species];
    for (uint8_t e = 0; e < d.evoCount; e++)
      if (!(s.variant ? w.pet.isVariantCollected(d.evos[e].to) : w.pet.isCollected(d.evos[e].to))) return true;
    return false;
  };
  const uint32_t step = 5, cap = 2u * 365 * 24 * 60;
  uint32_t t = 7 * 60;                 // the clock starts at 07:00
  for (; t < cap && w.pet.phase() != Phase::Complete; t += step) {
    uint32_t tod = t % 1440;
    bool night = sleepsAtNight && (tod >= 23 * 60 || tod < 7 * 60);
    if (!w.pet.isEgg() && w.pet.sleeping() != night) w.pet.toggleSleep();
    if (!night && t % careEvery == 0) {
      if (w.pet.careMistakes() > r.maxMistakes) r.maxMistakes = w.pet.careMistakes();
      w.pet.clean(w.now);
      w.pet.debugSetNeeds(100, 100, 100, 100);   // fed + played
      for (int act = 0; act < 4; act++) {        // a few decisions per check-in
        if (w.pet.canEvolve()) { w.pet.evolve(w.now); continue; }
        StoreError can = w.pet.canStoreActive();
        if (can != StoreError::Ok && can != StoreError::Duplicate) break;   // keep raising it
        int16_t slot = -1;                       // a keepsake's missing branch comes first
        for (uint8_t i = 0; i < w.pet.storageCount() && slot < 0; i++)
          if (openBranch(w.pet.stored(i))) slot = i;
        if (slot >= 0) {
          CHECK(w.pet.switchWith((uint8_t)slot, w.now, true) == StoreError::Ok);
          continue;
        }
        CHECK(w.pet.storeActiveAndNewEgg(w.now, true) == StoreError::Ok);
        if (!r.normalDoneMin && w.pet.phase() != Phase::Normal) r.normalDoneMin = t;
        uint16_t before = w.pet.collectedCount() + w.pet.variantCollectedCount();
        w.tap3();
        r.eggs++;
        if (w.pet.collectedCount() + w.pet.variantCollectedCount() == before) r.repeats++;
      }
    }
    w.pet.advanceMinutes(step);
  }
  r.totalMin = t;
  r.complete = w.pet.phase() == Phase::Complete &&
               w.pet.collectedCount() == kTheme.speciesCount && w.pet.variantCollectedCount() == kTheme.speciesCount;
  if (!r.normalDoneMin) r.normalDoneMin = t;
  return r;
}

void report(const char *who, const Playthrough &r) {
  std::printf("    %s: %u eggs, %u repeats, normal done %.1f d, everything %.1f d, worst mistakes %u\n", who,
              r.eggs, r.repeats, r.normalDoneMin / 1440.0, r.totalMin / 1440.0, r.maxMistakes);
}

}  // namespace

TEST(a_perfect_playthrough_completes_both_collections_without_a_repeat) {
  Playthrough r = playthrough(5, false);   // checks in every 5 minutes, never sleeps
  report("perfect care", r);
  CHECK(r.complete);
  CHECK_EQ(r.repeats, 0u);
  CHECK_EQ(r.maxMistakes, 0u);
}

TEST(a_realistic_playthrough_also_completes_without_a_repeat) {
  Playthrough r = playthrough(120, true);  // checks in every 2 h, asleep 23:00-07:00
  report("every 2 h + nights", r);
  CHECK(r.complete);
  CHECK_EQ(r.repeats, 0u);
}

TEST(stint_survives_a_reboot_and_counts_time_away) {
  World w;
  w.start("kitten");
  w.pet.setClock(1800000000u);
  w.pet.debugSetSpecies(sp("reimu"));
  w.pet.advanceMinutes(500);
  w.pet.save();
  Pet again(kTheme, w.kv, w.rng, w.cfg);
  again.begin(w.now);
  CHECK_EQ(again.stintMinutes(), 500u);
  CHECK(again.canStoreActive() == StoreError::TooYoung);
  again.syncClock(1800000000u + 60 * 60);   // an hour powered off
  CHECK(again.stintMinutes() >= 540u);
  CHECK(again.canStoreActive() == StoreError::Ok);
}
