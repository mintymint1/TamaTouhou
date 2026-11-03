#pragma once
// What a theme gives the engine: roster, rarity tiers, starters, tunables.
// The tables are generated from themes/<id>/ by tools/build_theme.py (lib/theme_gen/), so the
// engine itself never names a species, place or theme word (docs/themes.md).
#include <cstdint>

namespace tama {

constexpr uint8_t kTierEvo = 0xFF;        // species only reachable by evolving
constexpr uint16_t kNoSpecies = 0xFFFF;   // "egg" / none

struct EvoEdge {
  uint16_t to;      // species index
  uint8_t level;    // minimum level (care mistakes add to it)
};

struct SpeciesDef {
  const char *id;          // stable id from roster.yaml
  uint16_t key;            // hash16(id): what saves store, so roster order can change
  const char *name;        // display name
  uint8_t tier;            // index into ThemeDef::tiers, or kTierEvo
  uint8_t habitat;         // index into the theme's habitat table
  uint16_t accent565;      // header colour
  uint8_t favouriteFood;   // 0..2, the hidden favourite
  uint8_t kind;            // vocabulary kind (0 = default)
  uint8_t evoCount;
  const EvoEdge *evos;
};

struct TierDef {
  const char *id;          // e.g. "mythic"
  float odds;              // percent per egg (ignored for the catch-all tier)
  float blessedOdds;       // percent after a farewell
  uint16_t unlockCollected;  // collection size needed before this tier can roll
  bool catchAll;           // the last tier: gets whatever the others didn't
};

struct ThemeDef {
  const char *id;
  uint16_t version;
  const SpeciesDef *species;
  uint16_t speciesCount;    // <= 256
  const TierDef *tiers;     // rarest first, last one is the catch-all
  uint8_t tierCount;
  const uint16_t *starters; // species indices offered on the first run
  uint8_t starterCount;
  uint16_t variantOdds;     // 1 in N (before streak/bond bonuses)
  uint16_t farewellAgeDays;
};

// FNV-1a folded to 16 bits; must match tools/build_theme.py:hash16
inline uint16_t hash16(const char *s) {
  uint32_t h = 2166136261u;
  for (; *s; ++s) { h ^= (uint8_t)*s; h *= 16777619u; }
  return (uint16_t)((h >> 16) ^ (h & 0xFFFF));
}

}  // namespace tama
