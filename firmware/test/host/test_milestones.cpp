// Milestones for the Status screen and the need-rate query, against the real theme.
#include "framework.h"
#include "kv_mem.h"
#include "pet.h"
#include "theme_data.h"

using namespace tama;

namespace {

uint16_t sid(const char *id) {
  for (uint16_t i = 0; i < kTheme.speciesCount; i++)
    if (std::strcmp(kTheme.species[i].id, id) == 0) return i;
  failures()++;
  return 0;
}

struct World {
  MemKv kv;
  Rng rng{4242};
  Config cfg;
  uint32_t now = 1000;
  Pet pet{kTheme, kv, rng, cfg};
  void startKitten() {
    pet.begin(now);
    for (uint8_t i = 0; i < kTheme.starterCount; i++)
      if (kTheme.starters[i] == sid("kitten")) pet.chooseStarter(i);
    pet.eggTap(now); pet.eggTap(now); pet.eggTap(now);
  }
  uint8_t list(Milestone *m) { return pet.milestones(m, kMaxMilestones); }
  const Milestone *find(Milestone *m, uint8_t n, MilestoneKind k) {
    for (uint8_t i = 0; i < n; i++) if (m[i].kind == k) return &m[i];
    return nullptr;
  }
};

}  // namespace

TEST(egg_has_only_the_hatch_milestone) {
  World w;
  w.pet.begin(w.now);
  w.pet.chooseStarter(0);
  w.pet.eggTap(w.now);
  Milestone m[kMaxMilestones];
  uint8_t n = w.list(m);
  CHECK_EQ(n, 1);
  CHECK(m[0].kind == MilestoneKind::Hatch);
  CHECK(m[0].unit == MilestoneUnit::Taps);
  CHECK_EQ(m[0].current, 1u);
  CHECK_EQ(m[0].target, 3u);
  CHECK_EQ(m[0].etaMinutes, 3);
}

TEST(next_level_counts_minutes_into_the_hour) {
  World w;
  w.startKitten();
  w.pet.debugSetLevel(4);
  w.pet.advanceMinutes(20);
  Milestone m[kMaxMilestones];
  uint8_t n = w.list(m);
  const Milestone *x = w.find(m, n, MilestoneKind::NextLevel);
  CHECK(x != nullptr);
  if (!x) return;
  CHECK_EQ(x->current, 20u);
  CHECK_EQ(x->target, 60u);
  CHECK_EQ(x->etaMinutes, 40);
}

TEST(evolution_milestone_has_eta_blocks_and_branch_ref) {
  World w;
  w.startKitten();
  w.pet.debugSetLevel(14);
  w.pet.debugSetNeeds(30, 90, 90, 90);
  Milestone m[kMaxMilestones];
  uint8_t n = w.list(m);
  const Milestone *x = w.find(m, n, MilestoneKind::Evolution);
  CHECK(x != nullptr);
  if (!x) return;
  CHECK(x->unit == MilestoneUnit::Levels);
  CHECK_EQ(x->current, 14u);
  CHECK_EQ(x->target, 16u);
  CHECK_EQ(x->etaMinutes, 120);
  CHECK(x->blocked & MS_BLOCK_NEEDS);
  CHECK_EQ(x->ref, kNoSpecies);                   // Chen and Orin both still open
  w.pet.debugSetSpecies(sid("chen"));             // collect Chen...
  w.pet.debugSetSpecies(sid("kitten"));           // ...then back to a kitten
  w.pet.toggleSleep();
  n = w.list(m);
  x = w.find(m, n, MilestoneKind::Evolution);
  CHECK(x && x->ref == sid("orin"));              // only Orin is left to collect
  CHECK(x && (x->blocked & MS_BLOCK_ASLEEP));
}

TEST(care_mistakes_push_the_evolution_target) {
  World w;
  w.startKitten();
  w.pet.debugSetNeeds(5, 5, 5, 5);
  w.pet.advanceMinutes(1);                        // a need at or below 10: one mistake
  CHECK_EQ(w.pet.careMistakes(), 1);
  Milestone m[kMaxMilestones];
  uint8_t n = w.list(m);
  const Milestone *x = w.find(m, n, MilestoneKind::Evolution);
  CHECK(x && x->target == 17u);
}

TEST(final_form_counts_down_to_farewell) {
  World w;
  w.startKitten();
  w.pet.debugSetSpecies(sid("reimu"));
  w.pet.debugSetLevel(25);
  Milestone m[kMaxMilestones];
  uint8_t n = w.list(m);
  CHECK(w.find(m, n, MilestoneKind::Evolution) == nullptr);
  const Milestone *x = w.find(m, n, MilestoneKind::Farewell);
  CHECK(x != nullptr);
  if (!x) return;
  CHECK_EQ(x->target, (uint32_t)kTheme.farewellAgeDays * 1440);
  CHECK_EQ(x->etaMinutes, (int32_t)(x->target - x->current));
}

TEST(milestones_are_sorted_nearest_first) {
  World w;
  w.startKitten();
  w.pet.debugSetLevel(3);
  Milestone m[kMaxMilestones];
  uint8_t n = w.list(m);
  CHECK(n >= 5);
  bool seenOpen = false;
  for (uint8_t i = 0; i < n; i++) {
    if (m[i].etaMinutes < 0) { seenOpen = true; continue; }
    CHECK(!seenOpen);                             // every known ETA comes before open-ended ones
    if (i > 0 && m[i - 1].etaMinutes >= 0) CHECK(m[i - 1].etaMinutes <= m[i].etaMinutes);
  }
  CHECK(m[0].kind == MilestoneKind::NextLevel);   // the next level is always nearest
}

TEST(tier_unlocks_level_medal_and_collection_milestones) {
  World w;
  w.startKitten();
  w.pet.debugSetLevel(3);
  Milestone m[kMaxMilestones];
  uint8_t n = w.list(m);
  int tiers = 0;
  for (uint8_t i = 0; i < n; i++) {
    if (m[i].kind != MilestoneKind::TierUnlock) continue;
    tiers++;
    CHECK_EQ(m[i].target, (uint32_t)kTheme.tiers[m[i].ref].unlockCollected);
    CHECK_EQ(m[i].current, (uint32_t)w.pet.collectedCount());
    CHECK_EQ(m[i].etaMinutes, -1);
  }
  CHECK(tiers >= 2);                              // legendary (10) and mythic (30) still locked
  const Milestone *medal = w.find(m, n, MilestoneKind::LevelMedal);
  CHECK(medal && medal->target == 10u && medal->ref == MED_LV10);
  const Milestone *col = w.find(m, n, MilestoneKind::Collection);
  CHECK(col && col->target == (uint32_t)((w.pet.storageCapacity() * 25 + 99) / 100));
  CHECK(col && col->current == 1u);               // the active pet counts as owned
}

TEST(need_rates_follow_the_tick_rules) {
  World w;
  w.startKitten();
  int16_t r[4];
  w.pet.debugSetNeeds(80, 80, 80, 80);
  w.pet.needRatesPerHour(r);
  CHECK_EQ(r[0], -120);
  CHECK_EQ(r[1], -60);
  CHECK_EQ(r[2], -60);
  CHECK_EQ(r[3], -60 - 4 * 60 * (int)w.pet.mess());
  w.pet.debugSetNeeds(20, 80, 80, 20);            // hungry and dirty: joy drains faster
  w.pet.needRatesPerHour(r);
  CHECK_EQ(r[1], -300);
  w.pet.toggleSleep();
  w.pet.needRatesPerHour(r);
  CHECK_EQ(r[2], 360);
  CHECK_EQ(r[0], -30);
}
