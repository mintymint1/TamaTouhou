// Storage, keepsakes and switching (docs/design.md §4) against the real theme.
#include "framework.h"
#include "kv_mem.h"
#include "pet.h"
#include "theme_data.h"

using namespace tama;

namespace {

uint16_t sid(const char *id) {
  for (uint16_t i = 0; i < kTheme.speciesCount; i++)
    if (std::strcmp(kTheme.species[i].id, id) == 0) return i;
  std::printf("    (no species %s in theme)\n", id);
  failures()++;
  return 0;
}

constexpr uint32_t MIN = 60000;

struct World {
  MemKv kv;
  Rng rng{777};
  Config cfg;
  uint32_t now = 1000;
  Pet pet{kTheme, kv, rng, cfg};

  void minutes(uint32_t m) {
    for (uint32_t i = 0; i < m; i++) {
      now += MIN;
      pet.update(now);
    }
  }
  // first run with the kitten starter, hatched by tapping
  void startKitten() {
    pet.begin(now);
    for (uint8_t i = 0; i < kTheme.starterCount; i++)
      if (kTheme.starters[i] == sid("kitten")) pet.chooseStarter(i);
    pet.eggTap(now); pet.eggTap(now); pet.eggTap(now);
    if (pet.variant()) pet.debugToggleVariant();   // keep the colour predictable
  }
  // make the active pet ready to evolve at `lv` and evolve it
  void evolveAt(uint16_t lv) {
    pet.debugSetLevel(lv);
    pet.debugSetNeeds(100, 100, 100, 100);
    pet.evolve(now);
  }
};

}  // namespace

TEST(keepsake_on_evolution_keeps_the_previous_form) {
  World w;
  w.startKitten();
  w.pet.takeEvents();
  w.pet.rename("TAMA");
  w.evolveAt(16);
  uint16_t evolved = w.pet.species();
  CHECK(evolved == sid("chen") || evolved == sid("orin"));
  CHECK(w.pet.takeEvents() & EV_KEEPSAKE);
  CHECK_EQ(w.pet.storageCount(), 1);
  const Specimen &k = w.pet.stored(0);
  CHECK_EQ(k.species, sid("kitten"));
  CHECK(!k.variant);
  CHECK(k.origin == Origin::Keepsake);
  CHECK_EQ(k.keepsakeOf, evolved);
  CHECK(std::strcmp(k.keptFromNick, "TAMA") == 0);
  CHECK(k.nick[0] == 0);
  CHECK_EQ(w.pet.levelOf(k), 16);                 // keeps the level it evolved at
  CHECK_EQ(k.food, 100); CHECK_EQ(k.hygiene, 100);
  CHECK_EQ(k.bond, 0); CHECK_EQ(k.medals, 0); CHECK_EQ(k.careMistakes, 0);
}

TEST(keepsake_is_not_duplicated) {
  World w;
  w.startKitten();
  w.evolveAt(16);
  w.pet.takeEvents();
  w.pet.debugSetSpecies(sid("kitten"));          // a second kitten evolves too
  w.evolveAt(16);
  CHECK(!(w.pet.takeEvents() & EV_KEEPSAKE));
  CHECK_EQ(w.pet.storageCount(), 1);
}

TEST(storage_capacity_is_one_per_species_and_colour) {
  World w;
  w.startKitten();
  CHECK_EQ(w.pet.storageCapacity(), 2 * kTheme.speciesCount);
  w.pet.debugFillStorage();
  CHECK_EQ(w.pet.storageCount(), w.pet.storageCapacity());
  Specimen extra;
  extra.species = sid("reimu");
  CHECK(!w.pet.debugAddStored(extra));            // already there, and full
  CHECK_EQ(w.pet.ownedCount(), w.pet.storageCapacity());
}

TEST(storage_is_kept_in_roster_order) {
  World w;
  w.startKitten();
  for (const char *id : {"reimu", "kitten", "cirno"}) {
    Specimen s;
    s.species = sid(id);
    s.variant = std::strcmp(id, "kitten") == 0;
    CHECK(w.pet.debugAddStored(s));
  }
  Specimen plain;
  plain.species = sid("kitten");
  CHECK(w.pet.debugAddStored(plain));
  for (uint8_t i = 1; i < w.pet.storageCount(); i++) {
    const Specimen &a = w.pet.stored(i - 1), &b = w.pet.stored(i);
    CHECK(a.species < b.species || (a.species == b.species && !a.variant && b.variant));
  }
  CHECK_EQ(w.pet.findStored(sid("kitten"), false) + 1, w.pet.findStored(sid("kitten"), true));
}

TEST(stored_creatures_are_frozen) {
  World w;
  w.startKitten();
  w.evolveAt(16);
  uint32_t age = w.pet.stored(0).ageMinutes;
  w.minutes(600);                                 // ten hours of the active pet's life
  CHECK_EQ(w.pet.stored(0).ageMinutes, age);
  CHECK_EQ(w.pet.stored(0).food, 100);
  CHECK_EQ(w.pet.stored(0).mess, 0);
}

TEST(switch_swaps_active_and_stored_both_ways) {
  World w;
  w.startKitten();
  w.evolveAt(16);
  uint16_t evolved = w.pet.species();
  w.pet.debugSetNeeds(55, 66, 77, 88);
  uint32_t evolvedAge = w.pet.ageMinutes();
  w.pet.takeEvents();
  CHECK(w.pet.canSwitchWith(0) == StoreError::Ok);
  CHECK(w.pet.switchWith(0, w.now) == StoreError::Ok);
  CHECK(w.pet.takeEvents() & EV_SWITCH);
  CHECK_EQ(w.pet.species(), sid("kitten"));
  CHECK(w.pet.origin() == Origin::Keepsake);
  CHECK_EQ(w.pet.food(), 100);
  CHECK_EQ(w.pet.storageCount(), 1);
  CHECK_EQ(w.pet.stored(0).species, evolved);
  CHECK_EQ(w.pet.stored(0).food, 55);
  CHECK_EQ(w.pet.stored(0).hygiene, 88);
  CHECK(w.pet.switchWith(0, w.now) == StoreError::Ok);   // and back
  CHECK_EQ(w.pet.species(), evolved);
  CHECK_EQ(w.pet.joy(), 66);
  CHECK_EQ(w.pet.ageMinutes(), evolvedAge);
}

TEST(switched_in_pet_gets_no_catch_up_time) {
  World w;
  w.startKitten();
  w.evolveAt(16);
  w.minutes(120);
  uint32_t kittenAge = w.pet.stored(0).ageMinutes;
  w.pet.switchWith(0, w.now);
  w.pet.update(w.now);
  CHECK_EQ(w.pet.ageMinutes(), kittenAge);
  w.minutes(1);
  CHECK_EQ(w.pet.ageMinutes(), kittenAge + 1);
}

TEST(keepsake_lets_you_collect_the_other_branch) {
  World w;
  w.startKitten();
  w.evolveAt(16);
  uint16_t first = w.pet.species();
  w.pet.switchWith(0, w.now);                     // the kitten keepsake comes back out
  w.pet.debugSetNeeds(100, 100, 100, 100);
  CHECK(w.pet.canEvolve());                       // it kept its level
  w.pet.evolve(w.now);
  uint16_t second = w.pet.species();
  CHECK(second != first);                         // the branch not collected yet
  CHECK(w.pet.isCollected(sid("chen")));
  CHECK(w.pet.isCollected(sid("orin")));
  CHECK(w.pet.findStored(first, false) >= 0);
  CHECK(w.pet.findStored(sid("kitten"), false) >= 0);   // kept again: it wasn't stored now
  CHECK_EQ(w.pet.storageCount(), 2);
}

TEST(store_and_new_egg_is_gated_and_neutral) {
  World w;
  w.startKitten();
  w.pet.debugSetLevel(14);                        // the level alone doesn't earn a new egg
  CHECK(w.pet.canStoreActive() == StoreError::NotAwakened);
  CHECK(w.pet.storeActiveAndNewEgg(w.now) == StoreError::NotAwakened);
  w.evolveAt(16);                                 // evolving it does (the kitten is kept)
  uint16_t evolved = w.pet.species();
  w.pet.takeEvents();
  CHECK(w.pet.storeActiveAndNewEgg(w.now) == StoreError::Ok);
  uint32_t ev = w.pet.takeEvents();
  CHECK(ev & EV_STORED);
  CHECK(ev & EV_NEW_EGG);
  CHECK(w.pet.isEgg());
  CHECK(w.pet.lastEnding() == Ending::None);      // neither blessed nor cursed
  CHECK_EQ(w.pet.storageCount(), 2);              // the keepsake kitten + the evolved form
  int16_t slot = w.pet.findStored(evolved, false);
  CHECK(slot >= 0);
  CHECK(w.pet.stored((uint8_t)slot).origin == Origin::Hatched);
}

TEST(final_form_needs_time_with_you_before_storing) {
  World w;
  w.startKitten();
  w.pet.debugSetSpecies(sid("reimu"));
  w.pet.debugSetLevel(40);                        // its level doesn't count, time with you does
  CHECK(w.pet.canStoreActive() == StoreError::TooYoung);
  w.pet.advanceMinutes(9 * 60);
  CHECK(w.pet.canStoreActive() == StoreError::Ok);
}

TEST(storing_a_duplicate_needs_replace) {
  World w;
  w.startKitten();
  w.pet.debugSetLevel(12);
  w.pet.debugMarkAwakened();
  CHECK(w.pet.storeActiveAndNewEgg(w.now) == StoreError::Ok);
  w.pet.eggTap(w.now); w.pet.eggTap(w.now); w.pet.eggTap(w.now);
  w.pet.debugSetSpecies(sid("kitten"));
  if (w.pet.variant()) w.pet.debugToggleVariant();
  w.pet.debugSetLevel(20);
  w.pet.debugMarkAwakened();
  CHECK(w.pet.canStoreActive() == StoreError::Duplicate);
  CHECK(w.pet.storeActiveAndNewEgg(w.now) == StoreError::Duplicate);
  CHECK(w.pet.storeActiveAndNewEgg(w.now, true) == StoreError::Ok);
  CHECK_EQ(w.pet.storageCount(), 1);
  CHECK_EQ(w.pet.levelOf(w.pet.stored(0)), 20);  // the new one replaced the old one
}

TEST(switching_into_a_duplicate_needs_replace) {
  World w;
  w.startKitten();
  w.evolveAt(16);                                 // storage: kitten
  Specimen chen;
  chen.species = w.pet.species();                 // a stored copy of the active pet's kind
  Specimen other;
  other.species = sid("reimu");
  CHECK(w.pet.debugAddStored(chen));
  CHECK(w.pet.debugAddStored(other));
  uint8_t reimuSlot = (uint8_t)w.pet.findStored(sid("reimu"), false);
  CHECK(w.pet.canSwitchWith(reimuSlot) == StoreError::Duplicate);
  uint8_t sameSlot = (uint8_t)w.pet.findStored(chen.species, false);
  CHECK(w.pet.canSwitchWith(sameSlot) == StoreError::Ok);   // like for like is fine
  CHECK(w.pet.switchWith(reimuSlot, w.now, true) == StoreError::Ok);
  CHECK_EQ(w.pet.species(), sid("reimu"));
  CHECK_EQ(w.pet.storageCount(), 2);              // kitten + the pet that was active
}

TEST(egg_and_ceremonies_block_switching) {
  World w;
  w.pet.begin(w.now);                             // starter pick: busy, and an egg
  Specimen s;
  s.species = sid("reimu");
  CHECK(w.pet.debugAddStored(s));
  CHECK(w.pet.canSwitchWith(0) == StoreError::Egg);
  CHECK(w.pet.canSwitchWith(5) == StoreError::BadSlot);
  CHECK(w.pet.canStoreActive() == StoreError::Egg);
  World v;
  v.startKitten();
  CHECK(v.pet.debugAddStored(s));
  v.pet.release(v.now);                           // ending ceremony running
  CHECK(v.pet.canSwitchWith(0) == StoreError::Busy);
  CHECK(v.pet.canStoreActive() == StoreError::Busy);
}

TEST(release_from_storage_frees_the_slot) {
  World w;
  w.startKitten();
  w.evolveAt(16);
  CHECK(w.pet.releaseStored(0));
  CHECK_EQ(w.pet.storageCount(), 0);
  CHECK(!w.pet.releaseStored(0));
  CHECK(w.pet.isCollected(sid("kitten")));        // the Chronicle remembers it
}

TEST(storage_survives_a_reboot) {
  MemKv kv;
  Rng rng{5};
  Config cfg;
  uint16_t evolved;
  {
    World w;
    w.startKitten();
    w.pet.rename("MIKE");
    w.evolveAt(16);
    evolved = w.pet.species();
    kv = w.kv;
  }
  Pet p(kTheme, kv, rng, cfg);
  p.begin(5000);
  CHECK_EQ(p.species(), evolved);
  CHECK_EQ(p.storageCount(), 1);
  const Specimen &k = p.stored(0);
  CHECK_EQ(k.species, sid("kitten"));
  CHECK(k.origin == Origin::Keepsake);
  CHECK_EQ(k.keepsakeOf, evolved);
  CHECK(std::strcmp(k.keptFromNick, "MIKE") == 0);
  CHECK_EQ(p.levelOf(k), 16);
}

TEST(ui_settings_persist_with_the_deferred_save) {
  MemKv kv;
  Rng rng{9};
  Config cfg;
  {
    Pet p(kTheme, kv, rng, cfg);
    p.begin(1000);
    p.setUiFlags(0x13);
    p.setBrightness(5);
    CHECK(p.savePending());
    p.flushSave();
  }
  Pet q(kTheme, kv, rng, cfg);
  q.begin(2000);
  CHECK_EQ(q.uiFlags(), 0x13);
  CHECK_EQ(q.brightness(), 5);
}

// A save written by the v1 engine (before storage existed) must load unchanged.
TEST(v1_save_migrates_to_v2) {
  MemKv kv;
  uint8_t b[256];
  size_t n = 0;
  auto u8 = [&](uint8_t v) { b[n++] = v; };
  auto u16 = [&](uint16_t v) { u8(v & 0xFF); u8(v >> 8); };
  auto u32 = [&](uint32_t v) { u16(v & 0xFFFF); u16(v >> 16); };
  uint16_t kitten = kTheme.species[sid("kitten")].key;
  u32(0x54504554); u8(1); u16(hash16(kTheme.id));
  u8(61); u8(62); u8(63); u8(64); u8(1); u8(7);   // food joy energy hygiene mess weight
  u8(0x01);                                        // favouriteKnown
  u32(5 * 60);                                     // age: level 6
  u16(kitten); u16(0); u16(kitten);                // species, prev, egg target
  u8(0); u8(2); u8(0); u8(0);                      // eggTaps, mistakes, cooldown, neglect
  u8(0); u8(0);                                    // ceremony, lastEnd
  u32(1767225600);                                 // lastSeenEpoch
  u16(4); u16(9); u32(20454);                      // streak, best, lastCareDay
  u8(33); u8(4);                                   // bond, bondToday
  const char nick[kNickLen] = "OLDIE";
  for (uint8_t i = 0; i < kNickLen; i++) u8((uint8_t)nick[i]);
  u16(0x0001); u16(3); u16(3); u16(17);            // medals, total, lastMilestone, playHi
  u16(0); u32(0); u16(0);                          // evoDeclined, farDeclined, goodTicks
  u16(1); u16(kitten);                             // collected
  u16(0);                                          // collected variant
  kv.putBytes("pet", b, n);

  Rng rng{1};
  Config cfg;
  Pet p(kTheme, kv, rng, cfg);
  p.begin(1000);
  CHECK_EQ(p.species(), sid("kitten"));
  CHECK_EQ(p.food(), 61); CHECK_EQ(p.hygiene(), 64); CHECK_EQ(p.weight(), 7);
  CHECK_EQ(p.level(), 6);
  CHECK_EQ(p.careMistakes(), 2);
  CHECK_EQ(p.bond(), 33);
  CHECK_EQ(p.streak(), 4);
  CHECK(std::strcmp(p.nick(), "OLDIE") == 0);
  CHECK(p.isCollected(sid("kitten")));
  CHECK(p.origin() == Origin::Hatched);
  CHECK_EQ(p.storageCount(), 0);
  CHECK_EQ(p.uiFlags(), 0);
  CHECK_EQ(p.stintMinutes(), 5u * 60);             // v3 stint: hatched, so its whole life
  CHECK(!p.awakenedThisStint());                   // a base form hasn't evolved yet
  p.save();                                        // written back in the current format
  uint8_t out[1400];
  size_t len = kv.getBytes("pet", out, sizeof(out));
  CHECK(len > 4);
  CHECK_EQ(out[4], 3);
}

// --------------------------------------------------------------------------- the keepsake loop
// Regression: evolve -> switch the keepsake back -> evolve -> switch back... looped forever,
// replaying evolutions and medals. A keepsake now only evolves into forms not collected yet
// in its own colour.

namespace {
uint8_t milestoneCount(Pet &p, MilestoneKind k) {
  Milestone m[kMaxMilestones];
  uint8_t n = p.milestones(m, kMaxMilestones), c = 0;
  for (uint8_t i = 0; i < n; i++) c += m[i].kind == k;
  return c;
}
}  // namespace

TEST(keepsake_loop_ends_once_every_branch_is_collected) {
  World w;
  w.startKitten();
  w.evolveAt(16);                                   // hatched kitten -> first branch
  uint16_t first = w.pet.species();
  w.pet.switchWith((uint8_t)w.pet.findStored(sid("kitten"), false), w.now);
  w.pet.debugSetNeeds(100, 100, 100, 100);
  w.pet.evolve(w.now);                              // keepsake -> the other branch only
  uint16_t second = w.pet.species();
  CHECK(second != first);
  CHECK(second == sid("chen") || second == sid("orin"));
  // both branches collected: the kitten keepsake can't evolve any more
  w.pet.switchWith((uint8_t)w.pet.findStored(sid("kitten"), false), w.now);
  CHECK_EQ(w.pet.species(), sid("kitten"));
  w.pet.debugSetNeeds(100, 100, 100, 100);
  w.pet.takeEvents();
  CHECK(!w.pet.canEvolve());
  CHECK(!w.pet.wantEvolveButton());
  CHECK(w.pet.evolutionsExhausted());
  CHECK_EQ(w.pet.levelsToEvolve(), -1);
  CHECK_EQ(milestoneCount(w.pet, MilestoneKind::Evolution), 0);
  w.pet.evolve(w.now);                              // a no-op
  CHECK_EQ(w.pet.species(), sid("kitten"));
  CHECK(!(w.pet.takeEvents() & EV_EVOLVE));
}

TEST(linear_line_keepsake_cannot_evolve_again) {
  World w;
  w.pet.begin(w.now);
  for (uint8_t i = 0; i < kTheme.starterCount; i++)
    if (kTheme.starters[i] == sid("ice_fairy")) w.pet.chooseStarter(i);
  w.pet.eggTap(w.now); w.pet.eggTap(w.now); w.pet.eggTap(w.now);
  if (w.pet.variant()) w.pet.debugToggleVariant();
  w.evolveAt(16);
  CHECK_EQ(w.pet.species(), sid("cirno"));
  w.pet.switchWith((uint8_t)w.pet.findStored(sid("ice_fairy"), false), w.now);
  w.pet.debugSetNeeds(100, 100, 100, 100);
  CHECK(!w.pet.canEvolve());
  CHECK(w.pet.evolutionsExhausted());
}

TEST(alt_colour_keepsake_evolves_while_its_colour_is_uncollected) {
  World w;
  w.startKitten();
  w.evolveAt(16);                                   // normal Chen/Orin progress only
  Specimen alt;
  alt.species = sid("kitten");
  alt.variant = true;
  alt.origin = Origin::Keepsake;
  alt.ageMinutes = 15 * 60;                         // level 16
  CHECK(w.pet.debugAddStored(alt));
  w.pet.switchWith((uint8_t)w.pet.findStored(sid("kitten"), true), w.now);
  CHECK(w.pet.variant());
  w.pet.debugSetNeeds(100, 100, 100, 100);
  CHECK(w.pet.canEvolve());                         // no ALT COLOR Chen or Orin yet
  CHECK(!w.pet.evolutionsExhausted());
  w.pet.evolve(w.now);
  CHECK(w.pet.isVariantCollected(w.pet.species()));
}

TEST(hatched_pets_still_evolve_into_collected_forms) {
  World w;                                          // a kitten from an egg, both branches known
  w.startKitten();
  w.pet.debugSetSpecies(sid("chen"));
  w.pet.debugSetSpecies(sid("orin"));
  w.pet.debugSetSpecies(sid("kitten"));
  CHECK(w.pet.origin() == Origin::Hatched);
  w.pet.debugSetLevel(16);
  w.pet.debugSetNeeds(100, 100, 100, 100);
  CHECK(w.pet.canEvolve());                         // raising a new one is the cost: allowed
  CHECK(!w.pet.evolutionsExhausted());
}
