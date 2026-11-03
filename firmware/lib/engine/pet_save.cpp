// Persistence (the pet blob, versioned and migrated in readState) and save export/import.
#include "pet.h"

#include <cstring>
#include <initializer_list>

#include "pet_format.h"

namespace tama {

using namespace detail;

// --------------------------------------------------------------------------- persistence

void Pet::save() {
  ticksSinceSave_ = 0;
  pendingSave_ = false;
  uint8_t buf[kSaveMax];
  size_t n = writeState(buf, sizeof(buf));
  if (n) kv_.putBytes(kSaveKey, buf, n);
}

size_t Pet::writeState(uint8_t *buf, size_t cap) {
  Writer w(buf, cap);
  auto key = [&](uint16_t idx) { return idx < theme_.speciesCount ? theme_.species[idx].key : (uint16_t)0; };
  w.u32(kSaveMagic);
  w.u8(kSaveVersion);
  w.u16(hash16(theme_.id));
  w.u8(s_.food); w.u8(s_.joy); w.u8(s_.energy); w.u8(s_.hygiene); w.u8(s_.mess); w.u8(s_.weight);
  w.u8((uint8_t)(s_.favouriteKnown | s_.variant << 1 | s_.sleeping << 2 | s_.starterPick << 3 | s_.eggVariant << 4));
  w.u32(s_.ageMinutes);
  w.u16(isEgg() ? 0 : key(s_.species));
  w.u16(key(s_.prevSpecies));
  w.u16(key(s_.eggTarget));
  w.u8(s_.eggTaps); w.u8(s_.careMistakes); w.u8(s_.mistakeCooldown); w.u8(s_.neglectTicks);
  w.u8(s_.ceremony == (uint8_t)Ending::None ? 0 : s_.lastEnd);  // an ending in progress resumes as a fresh egg
  w.u8(s_.lastEnd);
  w.u32(s_.lastSeenEpoch);
  w.u16(s_.streak); w.u16(s_.bestStreak); w.u32(s_.lastCareDay);
  w.u8(s_.bond); w.u8(s_.bondToday);
  w.bytes(s_.nick, kNickLen);
  w.u16(s_.medals); w.u16(s_.totalMedals); w.u16(s_.lastMilestone); w.u16(s_.playHi);
  w.u16(s_.evoDeclinedLv); w.u32(s_.farDeclinedAge); w.u16(s_.goodTicks);
  for (const uint8_t *bm : {s_.collected, s_.collectedVariant}) {   // collection by key
    uint16_t n = 0;
    for (uint16_t i = 0; i < theme_.speciesCount; i++) n += testBit(bm, i);
    w.u16(n);
    for (uint16_t i = 0; i < theme_.speciesCount; i++)
      if (testBit(bm, i)) w.u16(theme_.species[i].key);
  }
  // v2
  w.u8(s_.origin);
  w.u16(key(s_.keepsakeOf));
  w.bytes(s_.keptFromNick, kNickLen);
  w.u16(s_.uiFlags);
  w.u8(s_.brightness);
  // v3
  w.u8((uint8_t)(s_.eggPick | s_.awakenedStint << 1));
  w.u32(s_.stintMinutes);
  return w.ok() && w.n <= cap ? w.n : 0;
}

bool Pet::load() {
  uint8_t buf[kSaveMax];
  size_t len = kv_.getBytes(kSaveKey, buf, sizeof(buf));
  return readState(buf, len);
}

bool Pet::readState(const uint8_t *buf, size_t len) {
  if (len < 8) return false;
  Reader r(buf, len);
  if (r.u32() != kSaveMagic) return false;
  uint8_t version = r.u8();
  if (version < 1 || version > kSaveVersion || r.u16() != hash16(theme_.id)) return false;
  State s;
  s.food = r.u8(); s.joy = r.u8(); s.energy = r.u8(); s.hygiene = r.u8(); s.mess = r.u8(); s.weight = r.u8();
  uint8_t flags = r.u8();
  s.favouriteKnown = flags & 1; s.variant = flags & 2; s.sleeping = flags & 4;
  s.starterPick = flags & 8; s.eggVariant = flags & 16;
  s.ageMinutes = r.u32();
  uint16_t spKey = r.u16(), prevKey = r.u16(), eggKey = r.u16();
  s.eggTaps = r.u8(); s.careMistakes = r.u8(); s.mistakeCooldown = r.u8(); s.neglectTicks = r.u8();
  uint8_t ceremony = r.u8();
  s.lastEnd = r.u8();
  s.lastSeenEpoch = r.u32();
  s.streak = r.u16(); s.bestStreak = r.u16(); s.lastCareDay = r.u32();
  s.bond = r.u8(); s.bondToday = r.u8();
  r.bytes(s.nick, kNickLen);
  s.nick[kNickLen - 1] = 0;
  s.medals = r.u16(); s.totalMedals = r.u16(); s.lastMilestone = r.u16(); s.playHi = r.u16();
  s.evoDeclinedLv = r.u16(); s.farDeclinedAge = r.u32(); s.goodTicks = r.u16();
  for (uint8_t *bm : {s.collected, s.collectedVariant}) {
    uint16_t n = r.u16();
    for (uint16_t k = 0; k < n && !r.bad; k++) {
      int16_t idx = indexOfKey(r.u16());
      if (idx >= 0) setBitIn(bm, (uint16_t)idx);   // species dropped from the roster: forgotten
    }
  }
  uint16_t keepsakeKey = 0;
  if (version >= 2) {           // v1 saves (before storage existed) keep the defaults
    s.origin = r.u8();
    keepsakeKey = r.u16();
    r.bytes(s.keptFromNick, kNickLen);
    s.keptFromNick[kNickLen - 1] = 0;
    s.uiFlags = r.u16();
    s.brightness = r.u8();
  }
  bool hasStint = version >= 3;
  if (hasStint) {
    uint8_t f3 = r.u8();
    s.eggPick = f3 & 1;
    s.awakenedStint = f3 & 2;
    s.stintMinutes = r.u32();
  }
  if (r.bad) return false;
  if (s.origin > (uint8_t)Origin::Keepsake) s.origin = 0;
  int16_t ko = keepsakeKey ? indexOfKey(keepsakeKey) : -1;
  s.keepsakeOf = ko >= 0 ? (uint16_t)ko : kNoSpecies;

  int16_t sp = spKey ? indexOfKey(spKey) : -1;
  int16_t prev = prevKey ? indexOfKey(prevKey) : -1;
  int16_t egg = eggKey ? indexOfKey(eggKey) : -1;
  s_ = s;
  s_.prevSpecies = prev >= 0 ? (uint16_t)prev : kNoSpecies;
  if (spKey && sp < 0) {        // the pet's species left the roster: start a fresh egg
    newEgg();
    return true;
  }
  s_.species = sp >= 0 ? (uint16_t)sp : kNoSpecies;
  if (!hasStint && !isEgg() && s_.origin == (uint8_t)Origin::Hatched) {
    // v1/v2 had no stint: a pet raised from its egg counts every minute of its life, and an
    // evolved form that hatched in your care has awakened.
    s_.stintMinutes = s_.ageMinutes;
    s_.awakenedStint = theme_.species[s_.species].tier == kTierEvo;
  }
  if (s_.eggPick) s_.eggTarget = kNoSpecies;
  else if (egg >= 0) s_.eggTarget = (uint16_t)egg;
  else if (isEgg()) prepareEgg();
  if (ceremony) newEgg();       // power was cut mid-ending: the ending is over
  if (!isEgg()) collect(s_.species);
  return true;
}

// --------------------------------------------------------------------------- save transfer

size_t Pet::exportSave(uint8_t *out, size_t cap) {
  if (!out || cap < kExportHeader + 4) return 0;
  ticksSinceSave_ = 0;
  pendingSave_ = false;
  size_t petLen = writeState(out + kExportHeader, cap - kExportHeader - 4);
  if (!petLen) return 0;
  kv_.putBytes(kSaveKey, out + kExportHeader, petLen);   // the export is also a save
  size_t boxLen = writeStorage(out + kExportHeader + petLen, cap - kExportHeader - petLen - 4);
  if (!boxLen) return 0;
  Writer w(out, cap);
  w.u32(kExportMagic);
  w.u8(kExportVersion);
  w.u16(hash16(theme_.id));
  w.u16((uint16_t)petLen);
  w.u16((uint16_t)boxLen);
  w.n = kExportHeader + petLen + boxLen;
  w.u32(crc32(out, w.n));
  return w.ok() ? w.n : 0;
}

ImportResult Pet::importSave(const uint8_t *data, size_t len, uint32_t nowMs) {
  if (!data || len < kExportHeader + 4) return ImportResult::BadFormat;
  Reader r(data, len);
  if (r.u32() != kExportMagic || r.u8() != kExportVersion) return ImportResult::BadFormat;
  uint16_t theme = r.u16();
  size_t petLen = r.u16(), boxLen = r.u16();
  if (kExportHeader + petLen + boxLen + 4 != len) return ImportResult::BadFormat;
  uint32_t stored = (uint32_t)data[len - 4] | (uint32_t)data[len - 3] << 8 | (uint32_t)data[len - 2] << 16 |
                    (uint32_t)data[len - 1] << 24;
  if (crc32(data, len - 4) != stored) return ImportResult::Corrupt;
  if (theme != hash16(theme_.id)) return ImportResult::WrongTheme;
  const uint8_t *pet = data + kExportHeader, *box = pet + petLen;
  if (petLen > kSaveMax || boxLen > kBoxMax || !readStorage(box, boxLen, false)) return ImportResult::Corrupt;
  if (!readState(pet, petLen)) return ImportResult::Corrupt;   // replaces s_ only on success
  readStorage(box, boxLen, true);
  eatUntil_ = heartUntil_ = evolveUntil_ = ceremonyUntil_ = medalUntil_ = milestoneUntil_ = 0;
  newMedal_ = 0;
  wasEvoReady_ = wasRunReady_ = false;
  lastTick_ = nowMs;
  save();
  saveStorage();
  events_ |= EV_LOADED;
  return ImportResult::Ok;
}

}  // namespace tama
