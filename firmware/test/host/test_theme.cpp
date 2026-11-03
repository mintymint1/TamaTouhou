// Integrity of the generated theme tables (firmware/lib/theme_gen, from themes/touhou/).
#include <set>

#include "framework.h"
#include "theme_data.h"

using namespace tama;

TEST(theme_has_species_tiers_and_starters) {
  CHECK(kTheme.speciesCount > 0);
  CHECK(kTheme.speciesCount <= 256);
  CHECK(kTheme.tierCount >= 2);
  CHECK(kTheme.tiers[kTheme.tierCount - 1].catchAll);
  for (uint8_t t = 0; t + 1 < kTheme.tierCount; t++) CHECK(!kTheme.tiers[t].catchAll);
  CHECK(kTheme.starterCount >= 1);
  for (uint8_t i = 0; i < kTheme.starterCount; i++) {
    CHECK(kTheme.starters[i] < kTheme.speciesCount);
    CHECK(kTheme.species[kTheme.starters[i]].tier != kTierEvo);   // starters are base forms
  }
}

TEST(theme_keys_are_unique_and_match_hash16) {
  std::set<uint16_t> keys;
  for (uint16_t i = 0; i < kTheme.speciesCount; i++) {
    const SpeciesDef &s = kTheme.species[i];
    CHECK_EQ(s.key, hash16(s.id));   // Python and C++ hashes agree
    CHECK(keys.insert(s.key).second);
  }
}

TEST(theme_evolution_edges_valid_and_targets_are_evo) {
  for (uint16_t i = 0; i < kTheme.speciesCount; i++) {
    const SpeciesDef &s = kTheme.species[i];
    CHECK(s.habitat < kHabitatCount);
    CHECK(s.favouriteFood <= 2);
    for (uint8_t e = 0; e < s.evoCount; e++) {
      CHECK(s.evos[e].to < kTheme.speciesCount);
      CHECK(kTheme.species[s.evos[e].to].tier == kTierEvo);
      CHECK(s.evos[e].level >= 2);
    }
  }
}

TEST(theme_every_tier_except_catch_all_has_base_forms) {
  for (uint8_t t = 0; t < kTheme.tierCount; t++) {
    int n = 0;
    for (uint16_t i = 0; i < kTheme.speciesCount; i++) n += kTheme.species[i].tier == t;
    CHECK(n > 0);
  }
}

TEST(theme_text_lookup_with_fallback) {
  CHECK(std::strlen(themeText("V_EGG", "")) > 0);
  CHECK(std::strcmp(themeText("S_DOES_NOT_EXIST", "fallback"), "fallback") == 0);
}
