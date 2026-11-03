#pragma once
// The pet: needs, ticks, eggs, evolution, endings, collection, streak/bond/medals.
// Hardware-free and theme-free: time comes in as arguments, randomness from an injected Rng,
// persistence through a KvStore, species data from a ThemeDef. Ported from TamaPoke's
// pet.cpp (MIT) with battle stats removed (decision D2) and data-driven rarity tiers.
#include <cstdint>

#include "platform.h"
#include "theme_def.h"

namespace tama {

struct Config {
  uint32_t tickMs = 60000;           // 1 tick = 1 game minute (lower it for fast tests)
  uint16_t minutesPerLevel = 60;     // +1 level per hour
  uint32_t eatAnimMs = 2500;
  uint32_t heartMs = 1500;
  uint32_t evolveAnimMs = 5200;
  uint32_t ceremonyMs = 10000;
  uint8_t runawayTicks = 60;         // all needs at 0 for an hour
  uint32_t offlineCapMin = 14UL * 24 * 60;
  uint8_t eggHatchMin = 3;           // hatches on its own after 3 minutes
  // "Store & start a new egg" for a pet with nothing left to awaken into: this many levels'
  // worth of time as the active pet first (9 = what a fresh hatch needs to reach Lv 10).
  uint8_t storeCareLevels = 9;
};

// Collection progress (docs/design.md §7). Normal and rare-colour
// (variant) entries are separate goals; eggs never repeat what's already collected.
//   Normal:   eggs roll species not collected in normal colour (a lucky variant is a bonus)
//   Variant:  every normal colour collected -> every egg is a variant not collected yet
//   Complete: both colours complete -> the player chooses the next egg
enum class Phase : uint8_t { Normal, Variant, Complete };

// Why importSave() refused a package (nothing was changed in any of these cases).
enum class ImportResult : uint8_t { Ok, BadFormat, WrongTheme, Corrupt };

enum class Mood : uint8_t { Happy, Sad, Eating, Sleeping };
enum class Ending : uint8_t { None = 0, Farewell, Runaway, Release };

// Per-pet medals (bitmask). No battle medals (D2).
enum : uint16_t {
  MED_LV10 = 1 << 0, MED_LV25 = 1 << 1, MED_LV50 = 1 << 2, MED_FAVOURITE = 1 << 3,
  MED_STREAK7 = 1 << 4, MED_BOND = 1 << 5, MED_FINAL = 1 << 6, MED_FIT = 1 << 7,
};
constexpr uint8_t kMedalCount = 8;

// Things the UI/audio react to. The engine never plays sounds or draws; it raises events
// and the firmware drains them with takeEvents().
enum : uint32_t {
  EV_HATCH = 1 << 0, EV_LEVEL_UP = 1 << 1, EV_EVOLVE = 1 << 2, EV_MEDAL = 1 << 3,
  EV_MILESTONE = 1 << 4, EV_HEART = 1 << 5, EV_ENDING = 1 << 6, EV_NEW_EGG = 1 << 7,
  EV_EVO_READY = 1 << 8, EV_RUNAWAY_READY = 1 << 9,
  EV_KEEPSAKE = 1 << 10,      // evolve() kept the previous form in storage
  EV_STORAGE_FULL = 1 << 11,  // evolve() couldn't keep it: storage full (a duplicate is silent)
  EV_SWITCH = 1 << 12,        // switchWith() swapped the active pet with a stored one
  EV_STORED = 1 << 13,        // storeActiveAndNewEgg() stored the pet (EV_NEW_EGG fires too)
  EV_PHASE = 1 << 14,         // a collection phase was completed (see phase())
  EV_LOADED = 1 << 15,        // importSave() replaced the whole game
};

constexpr uint16_t kMaxSpecies = 256;
constexpr uint8_t kNickLen = 12;     // including the terminator

// --------------------------------------------------------------------------- storage
// "Catch them all" (docs/design.md §4): evolving keeps the previous form
// as a keepsake, and any stored creature can be switched with the active pet. Stored
// creatures are frozen (never ticked): only the active pet needs care.
enum class Origin : uint8_t { Hatched = 0, Keepsake = 1 };

struct Specimen {
  uint16_t species = kNoSpecies;      // index into ThemeDef::species
  bool variant = false;               // rare colour
  Origin origin = Origin::Hatched;
  uint16_t keepsakeOf = kNoSpecies;   // Keepsake: the species the pet evolved INTO when kept
  char keptFromNick[kNickLen] = {0};  // Keepsake: that pet's nickname ("" = it had none)
  char nick[kNickLen] = {0};
  uint32_t ageMinutes = 0;
  uint8_t food = 100, joy = 100, energy = 100, hygiene = 100, mess = 0, weight = 0;
  bool favouriteKnown = false, sleeping = false;
  uint8_t careMistakes = 0, bond = 0, bondToday = 0;
  uint16_t medals = 0, evoDeclinedLv = 0, goodTicks = 0;
  uint32_t farDeclinedAge = 0;
};

constexpr uint8_t kStorageMax = 128;  // capacity is min(2 x species, this): one per colour

// Why a store/switch was refused. Duplicate: another stored copy of the active pet's
// species+colour exists; retry with replaceDuplicate = true to release that copy first.
// NotAwakened: it can still evolve, and a new egg is earned by evolving it. TooYoung:
// nothing left to evolve into, but not enough time as the active pet yet
// (storeReadyInMinutes()).
enum class StoreError : uint8_t { Ok, Egg, Busy, TooYoung, Full, Duplicate, BadSlot, NotAwakened };

// --------------------------------------------------------------------------- milestones
// "How far until the next thing" for the Status screen. Units are what current/target count.
enum class MilestoneKind : uint8_t {
  Hatch, NextLevel, Evolution, Farewell, Streak, Bond, LevelMedal, TierUnlock, Collection
};
enum class MilestoneUnit : uint8_t { Taps, Minutes, Levels, Days, Points, Count };
enum : uint8_t { MS_BLOCK_NEEDS = 1, MS_BLOCK_ASLEEP = 2 };   // Evolution: why it isn't ready

struct Milestone {
  MilestoneKind kind = MilestoneKind::NextLevel;
  MilestoneUnit unit = MilestoneUnit::Minutes;
  uint32_t current = 0, target = 0;  // progress bar = current / target
  int32_t etaMinutes = -1;           // -1 = depends on the player
  uint8_t blocked = 0;               // MS_BLOCK_* (Evolution only)
  uint16_t ref = 0;                  // Evolution: species it becomes (kNoSpecies = several
                                     // branches open) · TierUnlock: tier index ·
                                     // LevelMedal: medal bit · Streak: target days
};
constexpr uint8_t kMaxMilestones = 12;

class Pet {
 public:
  Pet(const ThemeDef &theme, KvStore &kv, Rng &rng, const Config &cfg = Config());

  // --- lifecycle
  void begin(uint32_t nowMs);                 // load the save, or start a new game
  void update(uint32_t nowMs);                // call every loop: ticks + ceremony end
  void syncClock(uint32_t nowEpoch);          // boot: apply time spent powered off
  // Hosts that suspend (a laptop lid under the simulator): call every loop before update().
  // The ms clock may pause or jump while the host sleeps; the wall clock doesn't. If it moved
  // >= 2 min since the last noteEpoch(), apply the gap like a boot (syncClock) and drop any
  // backlog of ms ticks so the time isn't counted twice. Returns true if it did.
  bool resumeIfSuspended(uint32_t nowMs, uint32_t nowEpoch);
  void setClock(uint32_t nowEpoch);           // clock set by the user / NTP, no progression
  void factoryReset(uint32_t nowMs);

  // --- actions (all no-ops when they don't apply)
  void chooseStarter(uint8_t i);              // first run only
  // Phase::Complete: the player picks the next egg (any base form, either colour). False if
  // no choice is pending or `species` isn't a base form.
  bool chooseEgg(uint16_t species, bool variant);
  void eggTap(uint32_t nowMs);
  void feed(uint8_t food, uint32_t nowMs);    // food 0..2; one is the species' favourite
  void feedTreat(uint32_t nowMs);
  void playResult(uint16_t score, uint32_t nowMs);
  void toggleSleep();
  void clean(uint32_t nowMs);
  void caress(uint32_t nowMs);
  void evolve(uint32_t nowMs);
  void declineEvolve() { s_.evoDeclinedLv = level(); }
  void startFarewell(uint32_t nowMs) { startEnding(Ending::Farewell, nowMs); }
  void declineFarewell() { s_.farDeclinedAge = s_.ageMinutes + 1440; }
  void startRunaway(uint32_t nowMs) { startEnding(Ending::Runaway, nowMs); }
  void release(uint32_t nowMs) { startEnding(Ending::Release, nowMs); }
  void rename(const char *nick);

  // --- queries
  bool isEgg() const { return s_.species == kNoSpecies; }
  bool awaitingStarter() const { return s_.starterPick; }
  bool awaitingEggChoice() const { return s_.eggPick; }
  uint16_t species() const { return s_.species; }
  uint16_t prevSpecies() const { return s_.prevSpecies; }
  const SpeciesDef *speciesDef() const { return isEgg() ? nullptr : &theme_.species[s_.species]; }
  bool variant() const { return s_.variant; }
  uint16_t level() const;
  Mood mood(uint32_t nowMs) const;
  uint8_t food() const { return s_.food; }
  uint8_t joy() const { return s_.joy; }
  uint8_t energy() const { return s_.energy; }
  uint8_t hygiene() const { return s_.hygiene; }
  uint8_t mess() const { return s_.mess; }
  uint8_t weight() const { return s_.weight; }
  uint8_t lowestNeed() const;
  bool sleeping() const { return s_.sleeping; }
  uint8_t eggTaps() const { return s_.eggTaps; }
  uint8_t eggTier() const;                    // tier of the hidden egg species (for the hint)
  uint32_t ageMinutes() const { return s_.ageMinutes; }
  uint8_t careMistakes() const { return s_.careMistakes; }
  bool favouriteKnown() const { return s_.favouriteKnown; }
  uint8_t bond() const { return s_.bond; }
  uint8_t bondToday() const { return s_.bondToday; }  // today's bond gain (daily cap 20)
  // How the needs change per game HOUR right now (food, joy, energy, hygiene; negative =
  // drain), from the same rules as tick(): awake incl. the mess/chubby/low-need modifiers,
  // or the slow asleep rates. Floors are not applied.
  void needRatesPerHour(int16_t out[4]) const;
  uint16_t streak() const { return s_.streak; }
  uint16_t bestStreak() const { return s_.bestStreak; }
  uint16_t medals() const { return s_.medals; }
  uint16_t totalMedals() const { return s_.totalMedals; }
  uint16_t playHigh() const { return s_.playHi; }
  const char *nick() const { return s_.nick; }
  Ending ending() const { return (Ending)s_.ceremony; }
  Ending lastEnding() const { return (Ending)s_.lastEnd; }
  uint32_t lastSeenEpoch() const { return s_.lastSeenEpoch; }
  void noteEpoch(uint32_t e) { if (e) s_.lastSeenEpoch = e; }

  bool eating(uint32_t now) const { return timeLeft(eatUntil_, now) > 0; }
  bool showHeart(uint32_t now) const { return timeLeft(heartUntil_, now) > 0; }
  bool evolving(uint32_t now) const { return timeLeft(evolveUntil_, now) > 0; }
  float evolveProgress(uint32_t now) const;   // 0..1
  float ceremonyProgress(uint32_t now) const; // 0..1
  bool showMedal(uint32_t now) const { return timeLeft(medalUntil_, now) > 0; }
  uint16_t newMedals() const { return newMedal_; }
  bool showMilestone(uint32_t now) const { return timeLeft(milestoneUntil_, now) > 0; }

  bool canEvolve() const;
  // A keepsake whose every evolution is already collected in its colour: it stays as it is
  // (keepsakes only evolve into forms not collected yet, which stops evolve/switch loops).
  bool evolutionsExhausted() const;
  bool wantEvolveButton() const { return canEvolve() && level() > s_.evoDeclinedLv; }
  int16_t levelsToEvolve() const;             // -1 = final form
  bool canFarewell() const;
  bool wantFarewellButton() const { return canFarewell() && s_.ageMinutes >= s_.farDeclinedAge; }
  bool canRunaway() const;

  // Collection: normal and rare colour are separate goals (a variant doesn't count as normal).
  uint16_t collectedCount() const;            // normal colour
  uint16_t variantCollectedCount() const;     // rare colour
  bool isCollected(uint16_t idx) const { return testBit(s_.collected, idx); }
  bool isVariantCollected(uint16_t idx) const { return testBit(s_.collectedVariant, idx); }
  bool isSeen(uint16_t idx) const { return isCollected(idx) || isVariantCollected(idx); }
  Phase phase() const;
  int careBonus() const;                      // 0..14 from streak + bond

  // --- storage (stored creatures never tick; see the Specimen comment above)
  uint8_t storageCount() const { return boxCount_; }
  uint16_t storageCapacity() const;           // min(2 x speciesCount, kStorageMax)
  const Specimen &stored(uint8_t slot) const; // roster order, normal before rare colour
  int16_t findStored(uint16_t species, bool variant) const;   // slot, or -1
  uint16_t levelOf(const Specimen &s) const;
  uint16_t ownedCount() const;                // distinct species x colour: storage + active
  Origin origin() const { return (Origin)s_.origin; }
  uint16_t keepsakeOf() const { return s_.keepsakeOf; }
  const char *keptFromNick() const { return s_.keptFromNick; }
  // Gate for "store & start a new egg": a new egg is earned by raising the active pet since
  // it last hatched or came out of storage. It must have evolved in that time, or, if it has
  // nothing left to evolve into, been the active pet for Config::storeCareLevels levels.
  // Switching a stored pet in and storing it again therefore never yields a free egg.
  StoreError canStoreActive() const;
  StoreError storeActiveAndNewEgg(uint32_t nowMs, bool replaceDuplicate = false);
  uint32_t storeReadyInMinutes() const;       // TooYoung: game minutes still needed, else 0
  bool awakenedThisStint() const { return s_.awakenedStint; }
  uint32_t stintMinutes() const { return s_.stintMinutes; }   // as the active pet, this time
  StoreError canSwitchWith(uint8_t slot) const;
  StoreError switchWith(uint8_t slot, uint32_t nowMs, bool replaceDuplicate = false);
  bool releaseStored(uint8_t slot);

  // --- milestones: nearest first (known ETA ascending, then the rest by % done)
  uint8_t milestones(Milestone *out, uint8_t max) const;

  // --- UI settings: opaque to the engine, persisted with the deferred save
  uint16_t uiFlags() const { return s_.uiFlags; }
  void setUiFlags(uint16_t f) { if (f != s_.uiFlags) { s_.uiFlags = f; pendingSave_ = true; } }
  uint8_t brightness() const { return s_.brightness; }
  void setBrightness(uint8_t b) { if (b != s_.brightness) { s_.brightness = b; pendingSave_ = true; } }

  uint32_t takeEvents() { uint32_t e = events_; events_ = 0; return e; }

  // --- persistence (one versioned blob; NVS writes are slow, see platform.h)
  bool savePending() const { return pendingSave_; }
  void flushSave() { if (pendingSave_) save(); }
  void save();

  // --- save transfer (backup, or moving a pet between the board and the simulator).
  // A package holds the pet blob and the storage blob with a CRC32; text encoding for files
  // and the serial console lives in save_text.h. importSave() validates everything (magic,
  // version, theme, CRC, both blobs) before it replaces anything, then persists both blobs.
  // Call syncClock() afterwards so the time since the export is applied like time powered off.
  static constexpr size_t kExportMax = 8192;  // enough for a full save + a full storage
  size_t exportSave(uint8_t *out, size_t cap);   // bytes written, 0 if `cap` is too small
  ImportResult importSave(const uint8_t *data, size_t len, uint32_t nowMs);

  // --- for tests and the serial console
  // Species for an egg in that colour, or kNoSpecies when nothing in that colour is left to
  // hatch. Never a species already collected in that colour, except a base form that was
  // lost (released, ran away) before its line was finished and that nothing owned can replace.
  uint16_t rollEggSpecies(bool variant = false);
  uint8_t rollTier(bool variant = false);     // unlocks count the collection in that colour
  void debugSetLevel(uint16_t lv) { s_.ageMinutes = (uint32_t)(lv - 1) * cfg_.minutesPerLevel; }
  void debugSetSpecies(uint16_t idx) { s_.prevSpecies = s_.species; s_.species = idx; collect(idx); }
  void debugSetNeeds(uint8_t f, uint8_t j, uint8_t e, uint8_t h) { s_.food = f; s_.joy = j; s_.energy = e; s_.hygiene = h; }
  void debugToggleVariant() { s_.variant = !s_.variant; if (!isEgg()) collect(s_.species); }
  // Test speed controls (PC simulator, serial console): game-minute length, instant skip.
  void setTickMs(uint32_t ms) { cfg_.tickMs = ms ? ms : 1; }
  uint32_t tickMs() const { return cfg_.tickMs; }
  void advanceMinutes(uint32_t minutes) { for (uint32_t i = 0; i < minutes; i++) tick(); }
  void debugRunawayReady() { s_.food = s_.joy = s_.energy = s_.hygiene = 0; s_.neglectTicks = cfg_.runawayTicks; }
  bool debugAddStored(const Specimen &s);     // respects capacity and one-per-colour
  void debugFillStorage();                    // every species x colour as Lv-1 keepsakes
  void debugMarkAwakened() { s_.awakenedStint = true; }   // as if it evolved this stint
  void debugCollect(uint16_t idx, bool variant);           // mark collected without hatching

 private:
  struct State {
    uint8_t food = 80, joy = 80, energy = 80, hygiene = 100, mess = 0, weight = 0;
    bool favouriteKnown = false, variant = false, sleeping = false, starterPick = false;
    uint32_t ageMinutes = 0;
    uint16_t species = kNoSpecies, prevSpecies = kNoSpecies;
    uint16_t eggTarget = 0;
    bool eggVariant = false;
    uint8_t eggTaps = 0;
    uint8_t careMistakes = 0, mistakeCooldown = 0, neglectTicks = 0;
    uint8_t ceremony = 0, lastEnd = 0;
    uint32_t lastSeenEpoch = 0;
    uint16_t streak = 0, bestStreak = 0;
    uint32_t lastCareDay = 0;
    uint8_t bond = 0, bondToday = 0;
    char nick[kNickLen] = {0};
    uint16_t medals = 0, totalMedals = 0, lastMilestone = 0, playHi = 0;
    uint16_t evoDeclinedLv = 0;
    uint32_t farDeclinedAge = 0;
    uint16_t goodTicks = 0;
    uint8_t collected[kMaxSpecies / 8] = {0};
    uint8_t collectedVariant[kMaxSpecies / 8] = {0};
    // save v2
    uint8_t origin = 0;                   // Origin of the active pet
    uint16_t keepsakeOf = kNoSpecies;
    char keptFromNick[kNickLen] = {0};
    uint16_t uiFlags = 0;
    uint8_t brightness = 0;
    // save v3
    bool eggPick = false;                 // Phase::Complete: waiting for chooseEgg()
    bool awakenedStint = false;           // evolved since it hatched / came out of storage
    uint32_t stintMinutes = 0;            // game minutes as the active pet since then
  };

  // Blob (de)serialisers shared by save()/load() and the export/import package. readState()
  // replaces s_ only when the whole blob parsed; readStorage(apply = false) only validates.
  size_t writeState(uint8_t *buf, size_t cap);
  bool readState(const uint8_t *buf, size_t len);
  size_t writeStorage(uint8_t *buf, size_t cap) const;
  bool readStorage(const uint8_t *buf, size_t len, bool apply);

  Specimen captureActive() const;             // the active pet as a storage record
  void restoreActive(const Specimen &sp);     // make a stored record the active pet
  bool insertStored(const Specimen &sp);      // keeps roster order; false if full
  void removeStored(uint8_t slot);
  void saveStorage();
  bool loadStorage();
  bool busy() const;
  void newEgg();
  void prepareEgg();                          // pick the next egg's species + colour (or a choice)
  bool luckyVariant();                        // the Normal-phase rare-colour roll
  void hatch(uint32_t nowMs);
  void tick();
  void startEnding(Ending e, uint32_t nowMs);
  void registerCare(uint32_t nowMs);
  void addBond(uint8_t amt);
  void checkMedals(uint32_t nowMs);
  void collect(uint16_t idx);
  bool collectedIn(uint16_t idx, bool variant) const { return variant ? isVariantCollected(idx) : isCollected(idx); }
  bool owns(uint16_t idx, bool variant) const;          // active pet or in storage
  bool eggEligible(uint16_t idx, bool variant) const;
  bool lineNeedsEgg(uint16_t idx, bool variant, int depth = 0) const;
  bool isBaseForm(uint16_t idx) const { return theme_.species[idx].tier != kTierEvo; }
  bool collectedInColour(uint16_t idx) const;
  bool evoTargetOpen(uint16_t to) const;
  bool canStillEvolve() const;                // has an evolution it may still take
  bool storeReady() const;
  uint32_t storeCareMinutes() const { return (uint32_t)cfg_.storeCareLevels * cfg_.minutesPerLevel; }
  int16_t indexOfKey(uint16_t key) const;
  bool load();
  uint32_t today() const { return s_.lastSeenEpoch ? s_.lastSeenEpoch / 86400 : 0; }
  static bool testBit(const uint8_t *bm, uint16_t i) { return i < kMaxSpecies && (bm[i >> 3] >> (i & 7)) & 1; }
  static void setBitIn(uint8_t *bm, uint16_t i) { if (i < kMaxSpecies) bm[i >> 3] |= (uint8_t)(1 << (i & 7)); }
  static uint8_t clamp100(int v) { return v < 0 ? 0 : (v > 100 ? 100 : (uint8_t)v); }
  static uint8_t dropTo(uint8_t v, uint8_t d, uint8_t floor);

  const ThemeDef &theme_;
  KvStore &kv_;
  Rng &rng_;
  Config cfg_;
  State s_;
  uint32_t lastTick_ = 0;
  uint32_t eatUntil_ = 0, heartUntil_ = 0, evolveUntil_ = 0, ceremonyUntil_ = 0;
  uint32_t medalUntil_ = 0, milestoneUntil_ = 0;
  uint16_t newMedal_ = 0;
  uint8_t ticksSinceSave_ = 0;
  bool pendingSave_ = false;
  uint32_t events_ = 0;
  bool wasEvoReady_ = false, wasRunReady_ = false;
  Specimen box_[kStorageMax];
  uint8_t boxCount_ = 0;
};

}  // namespace tama
