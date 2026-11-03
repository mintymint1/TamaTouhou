// Game rules (firmware/lib/engine/pet.cpp) against the real generated theme.
#include <cmath>

#include "framework.h"
#include "kv_mem.h"
#include "pet.h"
#include "theme_data.h"

using namespace tama;

namespace {

uint16_t idx(const char *id) {
  for (uint16_t i = 0; i < kTheme.speciesCount; i++)
    if (std::strcmp(kTheme.species[i].id, id) == 0) return i;
  std::printf("    (no species %s in theme)\n", id);
  failures()++;
  return 0;
}

constexpr uint32_t MIN = 60000;
constexpr uint32_t DAY_S = 86400;

struct World {
  MemKv kv;
  Rng rng{12345};
  Config cfg;
  uint32_t now = 1000;
  Pet pet{kTheme, kv, rng, cfg};

  void minutes(uint32_t m) {
    for (uint32_t i = 0; i < m; i++) {
      now += MIN;
      pet.update(now);
    }
  }
  // first run: pick starter i, hatch it by tapping (normal colour unless keepColour: the
  // two colours are separate collections, so a lucky rare colour would skew the counts)
  void startWith(uint8_t starter = 0, bool keepColour = false) {
    pet.begin(now);
    pet.chooseStarter(starter);
    pet.eggTap(now);
    pet.eggTap(now);
    pet.eggTap(now);
    if (!keepColour && pet.variant()) pet.debugToggleVariant();
  }
  void allNeeds(uint8_t v) { pet.debugSetNeeds(v, v, v, v); }
};

}  // namespace

// --------------------------------------------------------------------------- eggs / starters

TEST(first_run_offers_starters_and_freezes_time) {
  World w;
  w.pet.begin(w.now);
  CHECK(w.pet.awaitingStarter());
  CHECK(w.pet.isEgg());
  w.minutes(30);                       // TamaPoke 1.3: must not hatch while choosing
  CHECK(w.pet.isEgg());
  CHECK_EQ(w.pet.ageMinutes(), 0u);
  w.pet.chooseStarter(1);
  CHECK(!w.pet.awaitingStarter());
  w.pet.eggTap(w.now); w.pet.eggTap(w.now);
  CHECK(w.pet.isEgg());
  w.pet.eggTap(w.now);
  CHECK(!w.pet.isEgg());
  CHECK_EQ(w.pet.species(), kTheme.starters[1]);
  CHECK(w.pet.isCollected(kTheme.starters[1]));
  CHECK(w.pet.takeEvents() & EV_HATCH);
}

TEST(egg_hatches_by_itself_after_three_minutes) {
  World w;
  w.pet.begin(w.now);
  w.pet.chooseStarter(0);
  w.minutes(2);
  CHECK(w.pet.isEgg());
  w.minutes(1);
  CHECK(!w.pet.isEgg());
}

// --------------------------------------------------------------------------- needs

TEST(awake_needs_decay_per_minute) {
  World w;
  w.startWith();
  w.allNeeds(80);
  w.minutes(1);
  CHECK_EQ(w.pet.food(), 78);
  CHECK_EQ(w.pet.energy(), 79);
  CHECK(w.pet.hygiene() <= 79);        // -1, more if a mess appeared
  CHECK(w.pet.joy() <= 79);
}

TEST(sleep_restores_energy_and_needs_have_floors) {
  World w;
  w.startWith();
  w.pet.debugSetNeeds(31, 36, 10, 46);
  w.pet.toggleSleep();
  w.minutes(120);
  CHECK_EQ(w.pet.energy(), 100);
  CHECK_EQ(w.pet.food(), 30);
  CHECK_EQ(w.pet.joy(), 35);
  CHECK_EQ(w.pet.hygiene(), 45);
  CHECK(w.pet.mood(w.now) == Mood::Sleeping);
}

TEST(care_mistake_counts_once_per_hour) {
  World w;
  w.startWith();
  w.pet.debugSetNeeds(5, 100, 100, 100);
  w.minutes(1);
  CHECK_EQ(w.pet.careMistakes(), 1);
  w.pet.debugSetNeeds(5, 100, 100, 100);
  w.minutes(30);
  CHECK_EQ(w.pet.careMistakes(), 1);   // cooldown
  w.pet.debugSetNeeds(5, 100, 100, 100);
  w.minutes(31);
  CHECK_EQ(w.pet.careMistakes(), 2);
}

TEST(feeding_favourite_reveals_it) {
  World w;
  w.startWith();
  uint8_t fav = w.pet.speciesDef()->favouriteFood;
  w.allNeeds(50);
  w.pet.feed((uint8_t)((fav + 1) % 3), w.now);
  CHECK_EQ(w.pet.food(), 75);
  CHECK(!w.pet.favouriteKnown());
  w.allNeeds(50);
  w.pet.feed(fav, w.now);
  CHECK_EQ(w.pet.food(), 85);
  CHECK_EQ(w.pet.joy(), 60);
  CHECK(w.pet.favouriteKnown());
  CHECK(w.pet.showHeart(w.now));
  CHECK(w.pet.mood(w.now) == Mood::Eating);
}

TEST(treat_fattens_and_play_burns_weight) {
  World w;
  w.startWith();
  for (int i = 0; i < 5; i++) w.pet.feedTreat(w.now);
  CHECK_EQ(w.pet.weight(), 60);
  w.pet.playResult(20, w.now);
  CHECK_EQ(w.pet.weight(), 20);
  CHECK_EQ(w.pet.playHigh(), 20);
}

TEST(clean_resets_mess_and_hygiene) {
  World w;
  w.startWith();
  w.allNeeds(20);
  w.pet.clean(w.now);
  CHECK_EQ(w.pet.hygiene(), 100);
  CHECK_EQ(w.pet.mess(), 0);
}

// --------------------------------------------------------------------------- level / evolution

TEST(level_is_hours_and_caps_at_999) {
  World w;
  w.startWith();
  w.pet.debugSetLevel(17);
  CHECK_EQ(w.pet.level(), 17);
  w.pet.debugSetLevel(5000);
  CHECK_EQ(w.pet.level(), 999);
}

TEST(evolution_needs_level_and_good_care) {
  World w;
  w.startWith();
  w.pet.debugSetSpecies(idx("ice_fairy"));
  w.allNeeds(90);
  w.pet.debugSetLevel(15);
  CHECK(!w.pet.canEvolve());
  CHECK_EQ(w.pet.levelsToEvolve(), 1);
  w.pet.debugSetLevel(16);
  CHECK(w.pet.canEvolve());
  w.allNeeds(39);
  CHECK(!w.pet.canEvolve());            // any need < 40 blocks it
  w.allNeeds(90);
  w.pet.evolve(w.now);
  CHECK_EQ(w.pet.species(), idx("cirno"));
  CHECK_EQ(w.pet.prevSpecies(), idx("ice_fairy"));
  CHECK(w.pet.evolving(w.now));
  CHECK(w.pet.isCollected(idx("cirno")));
}

TEST(each_care_mistake_delays_evolution_by_a_level) {
  World w;
  w.startWith();
  w.pet.debugSetSpecies(idx("ice_fairy"));
  w.pet.debugSetNeeds(5, 100, 100, 100);
  w.minutes(1);                          // one mistake
  w.allNeeds(90);
  w.pet.debugSetLevel(16);
  CHECK(!w.pet.canEvolve());
  w.pet.debugSetLevel(17);
  CHECK(w.pet.canEvolve());
}

TEST(branch_evolution_prefers_uncollected_form) {
  World w;
  w.startWith();
  w.pet.debugSetSpecies(idx("chen"));    // collect chen
  w.pet.debugSetSpecies(idx("kitten"));
  w.allNeeds(90);
  w.pet.debugSetLevel(16);
  w.pet.evolve(w.now);
  CHECK_EQ(w.pet.species(), idx("orin"));
}

TEST(final_form_has_no_evolution_and_earns_medal) {
  World w;
  w.startWith();
  w.pet.debugSetSpecies(idx("reimu"));
  w.allNeeds(90);
  w.pet.debugSetLevel(60);
  CHECK(!w.pet.canEvolve());
  CHECK_EQ(w.pet.levelsToEvolve(), -1);
  w.minutes(1);
  CHECK(w.pet.medals() & MED_FINAL);
  CHECK(w.pet.medals() & MED_LV50);
}

TEST(declined_evolution_reoffers_next_level) {
  World w;
  w.startWith();
  w.pet.debugSetSpecies(idx("ice_fairy"));
  w.allNeeds(90);
  w.pet.debugSetLevel(16);
  CHECK(w.pet.wantEvolveButton());
  w.pet.declineEvolve();
  CHECK(!w.pet.wantEvolveButton());
  w.pet.debugSetLevel(17);
  CHECK(w.pet.wantEvolveButton());
}

// --------------------------------------------------------------------------- rarity tiers

TEST(rare_tiers_locked_until_collection_size) {
  World w;
  w.startWith();
  uint8_t mythic = 0;   // tiers are rarest first
  CHECK(std::strcmp(kTheme.tiers[mythic].id, "mythic") == 0);
  for (int i = 0; i < 3000; i++) CHECK(w.pet.rollTier() != mythic);   // 1 collected < 30
  for (uint16_t i = 0; i < kTheme.speciesCount && w.pet.collectedCount() < 35; i++) w.pet.debugSetSpecies(i);
  int hits = 0;
  for (int i = 0; i < 20000; i++) hits += w.pet.rollTier() == mythic;
  double pct = 100.0 * hits / 20000;
  CHECK(pct > 0.6 && pct < 1.5);         // 1% base (bond/streak 0 here)
}

TEST(mythic_eggs_are_reimu_or_marisa_and_finish_the_hunt) {
  World w;
  w.startWith();
  for (uint16_t i = 0; i < kTheme.speciesCount; i++)   // everything else collected
    if (i != idx("reimu") && i != idx("marisa")) w.pet.debugSetSpecies(i);
  for (int i = 0; i < 4000; i++) {
    uint16_t s = w.pet.rollEggSpecies();
    CHECK(s == idx("reimu") || s == idx("marisa"));   // the last two: every egg, never a repeat
    if (s == kNoSpecies) break;
    CHECK(kTheme.species[s].tier == 0);
  }
}

TEST(runaway_curses_next_egg_to_common) {
  World w;
  w.startWith();
  for (uint16_t i = 0; i < kTheme.speciesCount; i++) w.pet.debugSetSpecies(i);
  w.pet.debugSetSpecies(idx("cirno"));
  w.pet.debugRunawayReady();
  CHECK(w.pet.canRunaway());
  w.pet.startRunaway(w.now);
  uint8_t common = kTheme.tierCount - 1;
  for (int i = 0; i < 2000; i++) CHECK_EQ(w.pet.rollTier(), common);
}

TEST(variant_rate_is_about_one_in_48) {
  int variants = 0;
  for (uint32_t seed = 1; seed <= 6000; seed++) {
    World w;
    w.rng.seed(seed * 2654435761u);
    w.startWith(0, true);
    variants += w.pet.variant();
  }
  double rate = variants / 6000.0;
  CHECK(rate > 1.0 / 70 && rate < 1.0 / 32);
}

// --------------------------------------------------------------------------- endings

TEST(runaway_needs_an_hour_of_total_neglect_and_care_cancels) {
  World w;
  w.startWith();
  w.allNeeds(0);
  w.minutes(59);
  w.allNeeds(0);
  CHECK(!w.pet.canRunaway());
  w.minutes(1);
  CHECK(w.pet.canRunaway());
  w.pet.caress(w.now);                   // joy 5: not "all zero" any more
  w.minutes(1);
  CHECK(!w.pet.canRunaway());
}

TEST(farewell_after_three_days_in_final_form_then_new_egg) {
  World w;
  w.startWith();
  w.pet.debugSetSpecies(idx("marisa"));
  CHECK(!w.pet.canFarewell());
  w.pet.debugSetLevel(3 * 24 + 1);
  CHECK(w.pet.canFarewell());
  w.pet.startFarewell(w.now);
  CHECK(w.pet.ending() == Ending::Farewell);
  w.now += 9000; w.pet.update(w.now);
  CHECK(!w.pet.isEgg());
  w.now += 2000; w.pet.update(w.now);
  CHECK(w.pet.isEgg());
  CHECK(w.pet.lastEnding() == Ending::Farewell);
  CHECK(!w.pet.awaitingStarter());       // collection not empty: no starter pick
}

// --------------------------------------------------------------------------- streak / bond

TEST(daily_care_builds_streak_and_bond) {
  World w;
  w.startWith();
  uint32_t epoch = 1767225600u;          // 2026-01-01
  for (int d = 0; d < 7; d++) {
    w.pet.setClock(epoch + d * DAY_S);
    w.pet.caress(w.now);
  }
  CHECK_EQ(w.pet.streak(), 7);
  CHECK(w.pet.medals() & MED_STREAK7);
  w.pet.setClock(epoch + 9 * DAY_S);     // skipped a day
  w.pet.caress(w.now);
  CHECK_EQ(w.pet.streak(), 1);
  CHECK_EQ(w.pet.bestStreak(), 7);
}

// --------------------------------------------------------------------------- time

TEST(ticks_survive_millis_wrap) {
  World w;
  w.now = 0xFFFFFFFFu - 5 * MIN;
  w.startWith();
  w.allNeeds(80);
  w.minutes(10);                         // crosses the 2^32 wrap
  CHECK_EQ(w.pet.food(), 60);
}

TEST(offline_progression_has_floors_and_cap_and_no_evolution) {
  World w;
  w.startWith();
  w.pet.debugSetSpecies(idx("ice_fairy"));
  uint32_t epoch = 1767225600u;
  w.pet.setClock(epoch);
  w.pet.syncClock(epoch + 30 * DAY_S);   // a month away, capped at 14 days
  CHECK_EQ(w.pet.ageMinutes(), 14u * 24 * 60);
  CHECK_EQ(w.pet.food(), 15);
  CHECK_EQ(w.pet.careMistakes(), 0);
  CHECK_EQ(w.pet.species(), idx("ice_fairy"));   // evolution waits for the player
  CHECK(!w.pet.canRunaway());
}

TEST(resume_after_host_suspend_counts_the_gap_once) {
  World w;
  w.startWith();
  w.pet.debugSetSpecies(idx("ice_fairy"));
  w.allNeeds(80);
  uint32_t epoch = 1767225600u;
  w.pet.setClock(epoch);
  uint32_t age = w.pet.ageMinutes();

  // macOS: the ms clock paused while the lid was shut, the wall clock moved 8 hours
  CHECK(w.pet.resumeIfSuspended(w.now, epoch + 8 * 3600));
  CHECK_EQ(w.pet.ageMinutes(), age + 480);
  CHECK_EQ(w.pet.careMistakes(), 0);             // offline rules: no mistakes while away
  CHECK(!w.pet.resumeIfSuspended(w.now, epoch + 8 * 3600 + 60));   // a normal loop: no gap

  // the ms clock kept running instead: the resume drops that backlog of ticks
  epoch += 8 * 3600 + 60;
  w.pet.noteEpoch(epoch);
  age = w.pet.ageMinutes();
  w.now += 60 * MIN;
  CHECK(w.pet.resumeIfSuspended(w.now, epoch + 3600));
  w.pet.update(w.now);
  CHECK_EQ(w.pet.ageMinutes(), age + 60);        // not 120

  // the clock going backwards, or no clock yet, is never a resume
  CHECK(!w.pet.resumeIfSuspended(w.now, epoch));
  CHECK(!w.pet.resumeIfSuspended(w.now, 0));
}

// --------------------------------------------------------------------------- saves

TEST(save_and_reload_roundtrip) {
  MemKv kv;
  Rng rng(7);
  uint32_t now = 1000;
  {
    Pet p(kTheme, kv, rng);
    p.begin(now);
    p.chooseStarter(2);
    p.eggTap(now); p.eggTap(now); p.eggTap(now);
    p.debugSetSpecies(idx("satori"));
    p.debugSetNeeds(11, 22, 33, 44);
    p.rename("KOMEIJI");
    p.save();
  }
  Pet q(kTheme, kv, rng);
  q.begin(now);
  CHECK_EQ(q.species(), idx("satori"));
  CHECK_EQ(q.food(), 11);
  CHECK_EQ(q.hygiene(), 44);
  CHECK(std::strcmp(q.nick(), "KOMEIJI") == 0);
  CHECK(q.isCollected(kTheme.starters[2]));
  CHECK(q.isCollected(idx("satori")));
  CHECK(!q.awaitingStarter());
  CHECK(kv.size("pet") < 1400);
}

TEST(save_written_as_one_blob_and_periodic_saves_are_deferred) {
  World w;
  w.startWith();
  int before = w.kv.writes;
  w.minutes(4);
  CHECK_EQ(w.kv.writes, before);          // ticks don't write flash directly
  w.minutes(1);
  CHECK(w.pet.savePending());
  w.pet.flushSave();
  CHECK_EQ(w.kv.writes, before + 1);
  CHECK(!w.pet.savePending());
}

TEST(save_from_another_theme_is_ignored) {
  MemKv kv;
  uint8_t junk[64] = {0x54, 0x45, 0x50, 0x54, 1, 0x12, 0x34};   // right magic, wrong theme hash
  kv.putBytes("pet", junk, sizeof(junk));
  Rng rng(1);
  Pet p(kTheme, kv, rng);
  p.begin(0);
  CHECK(p.awaitingStarter());             // fresh game
}
