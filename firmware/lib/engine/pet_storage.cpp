// Storage: keepsakes, store & new egg, switching, release, and the storage blob.
#include "pet.h"

#include <cstring>
#include <initializer_list>

#include "pet_format.h"

namespace tama {

using namespace detail;

// --------------------------------------------------------------------------- storage

bool Pet::busy() const {
  return s_.starterPick || s_.ceremony != (uint8_t)Ending::None;
}

uint16_t Pet::storageCapacity() const {
  uint32_t c = 2u * theme_.speciesCount;
  return (uint16_t)(c < kStorageMax ? c : kStorageMax);
}

const Specimen &Pet::stored(uint8_t slot) const {
  static const Specimen kEmpty;
  return slot < boxCount_ ? box_[slot] : kEmpty;
}

int16_t Pet::findStored(uint16_t species, bool variant) const {
  for (uint8_t i = 0; i < boxCount_; i++)
    if (box_[i].species == species && box_[i].variant == variant) return i;
  return -1;
}

uint16_t Pet::levelOf(const Specimen &sp) const {
  uint32_t lv = 1 + sp.ageMinutes / cfg_.minutesPerLevel;
  return lv > 999 ? 999 : (uint16_t)lv;
}

uint16_t Pet::ownedCount() const {
  uint16_t n = boxCount_;   // storage already holds at most one per species x colour
  if (!isEgg() && findStored(s_.species, s_.variant) < 0) n++;
  return n;
}

Specimen Pet::captureActive() const {
  Specimen sp;
  sp.species = s_.species;
  sp.variant = s_.variant;
  sp.origin = (Origin)s_.origin;
  sp.keepsakeOf = s_.keepsakeOf;
  std::memcpy(sp.keptFromNick, s_.keptFromNick, kNickLen);
  std::memcpy(sp.nick, s_.nick, kNickLen);
  sp.ageMinutes = s_.ageMinutes;
  sp.food = s_.food; sp.joy = s_.joy; sp.energy = s_.energy; sp.hygiene = s_.hygiene;
  sp.mess = s_.mess; sp.weight = s_.weight;
  sp.favouriteKnown = s_.favouriteKnown;
  sp.sleeping = s_.sleeping;
  sp.careMistakes = s_.careMistakes;
  sp.bond = s_.bond; sp.bondToday = s_.bondToday;
  sp.medals = s_.medals;
  sp.evoDeclinedLv = s_.evoDeclinedLv;
  sp.goodTicks = s_.goodTicks;
  sp.farDeclinedAge = s_.farDeclinedAge;
  return sp;
}

void Pet::restoreActive(const Specimen &sp) {
  s_.species = sp.species;
  s_.prevSpecies = kNoSpecies;
  s_.variant = sp.variant;
  s_.origin = (uint8_t)sp.origin;
  s_.keepsakeOf = sp.keepsakeOf;
  std::memcpy(s_.keptFromNick, sp.keptFromNick, kNickLen);
  std::memcpy(s_.nick, sp.nick, kNickLen);
  s_.ageMinutes = sp.ageMinutes;
  s_.food = sp.food; s_.joy = sp.joy; s_.energy = sp.energy; s_.hygiene = sp.hygiene;
  s_.mess = sp.mess; s_.weight = sp.weight;
  s_.favouriteKnown = sp.favouriteKnown;
  s_.sleeping = sp.sleeping;
  s_.careMistakes = sp.careMistakes;
  s_.bond = sp.bond; s_.bondToday = sp.bondToday;
  s_.medals = sp.medals;
  s_.evoDeclinedLv = sp.evoDeclinedLv;
  s_.goodTicks = sp.goodTicks;
  s_.farDeclinedAge = sp.farDeclinedAge;
  s_.mistakeCooldown = 0;
  s_.neglectTicks = 0;      // the neglect clock belongs to the pet that was out
  s_.awakenedStint = false; // a new stint: raising it again is what earns a new egg
  s_.stintMinutes = 0;
  eatUntil_ = heartUntil_ = evolveUntil_ = 0;
  newMedal_ = 0;
  collect(s_.species);
}

bool Pet::insertStored(const Specimen &sp) {
  if (boxCount_ >= storageCapacity() || sp.species >= theme_.speciesCount) return false;
  uint8_t at = boxCount_;   // roster order: species index, then normal before rare colour
  for (uint8_t i = 0; i < boxCount_; i++) {
    const Specimen &b = box_[i];
    if (b.species > sp.species || (b.species == sp.species && b.variant > sp.variant)) {
      at = i;
      break;
    }
  }
  for (uint8_t i = boxCount_; i > at; i--) box_[i] = box_[i - 1];
  box_[at] = sp;
  boxCount_++;
  return true;
}

void Pet::removeStored(uint8_t slot) {
  if (slot >= boxCount_) return;
  for (uint8_t i = slot; i + 1 < boxCount_; i++) box_[i] = box_[i + 1];
  boxCount_--;
  box_[boxCount_] = Specimen();
}

bool Pet::canStillEvolve() const {
  return !isEgg() && theme_.species[s_.species].evoCount > 0 && !evolutionsExhausted();
}

// A new egg is earned by raising the active pet since it hatched or left storage: evolving
// it, or (when it has nothing left to evolve into) spending storeCareLevels levels with it.
bool Pet::storeReady() const {
  if (s_.awakenedStint) return true;
  if (canStillEvolve()) return false;
  return s_.stintMinutes >= storeCareMinutes();
}

uint32_t Pet::storeReadyInMinutes() const {
  if (isEgg() || storeReady() || canStillEvolve()) return 0;
  return storeCareMinutes() - s_.stintMinutes;
}

StoreError Pet::canStoreActive() const {
  if (isEgg()) return StoreError::Egg;
  if (busy()) return StoreError::Busy;
  if (!storeReady()) return canStillEvolve() ? StoreError::NotAwakened : StoreError::TooYoung;
  if (findStored(s_.species, s_.variant) >= 0) return StoreError::Duplicate;
  if (boxCount_ >= storageCapacity()) return StoreError::Full;
  return StoreError::Ok;
}

StoreError Pet::storeActiveAndNewEgg(uint32_t nowMs, bool replaceDuplicate) {
  StoreError e = canStoreActive();
  if (e == StoreError::Duplicate && replaceDuplicate) {
    removeStored((uint8_t)findStored(s_.species, s_.variant));
    e = canStoreActive();
  }
  if (e != StoreError::Ok) return e;
  insertStored(captureActive());
  // A neutral ending: the next egg is neither blessed (farewell) nor cursed (runaway).
  s_.lastEnd = (uint8_t)Ending::None;
  s_.origin = (uint8_t)Origin::Hatched;
  s_.keepsakeOf = kNoSpecies;
  s_.keptFromNick[0] = 0;
  s_.nick[0] = 0;
  eatUntil_ = heartUntil_ = evolveUntil_ = 0;
  newEgg();
  lastTick_ = nowMs;
  events_ |= EV_STORED;
  save();
  saveStorage();
  return StoreError::Ok;
}

StoreError Pet::canSwitchWith(uint8_t slot) const {
  if (slot >= boxCount_) return StoreError::BadSlot;
  if (isEgg()) return StoreError::Egg;
  if (busy()) return StoreError::Busy;
  int16_t dup = findStored(s_.species, s_.variant);
  if (dup >= 0 && dup != slot) return StoreError::Duplicate;   // swapping like for like is fine
  return StoreError::Ok;
}

StoreError Pet::switchWith(uint8_t slot, uint32_t nowMs, bool replaceDuplicate) {
  StoreError e = canSwitchWith(slot);
  if (e == StoreError::Duplicate && replaceDuplicate) {
    int16_t dup = findStored(s_.species, s_.variant);
    removeStored((uint8_t)dup);
    if (dup < slot) slot--;
    e = canSwitchWith(slot);
  }
  if (e != StoreError::Ok) return e;
  Specimen incoming = box_[slot];
  removeStored(slot);
  insertStored(captureActive());
  restoreActive(incoming);
  lastTick_ = nowMs;   // the stored pet was frozen: no catch-up ticks for its time away
  events_ |= EV_SWITCH;
  save();
  saveStorage();
  return StoreError::Ok;
}

bool Pet::releaseStored(uint8_t slot) {
  if (slot >= boxCount_) return false;
  removeStored(slot);
  saveStorage();
  return true;
}

bool Pet::debugAddStored(const Specimen &sp) {
  if (findStored(sp.species, sp.variant) >= 0) return false;
  if (!insertStored(sp)) return false;
  saveStorage();
  return true;
}

void Pet::debugFillStorage() {
  for (uint16_t i = 0; i < theme_.speciesCount; i++) {
    for (int v = 0; v < 2; v++) {
      if (findStored(i, v) >= 0) continue;
      Specimen sp;
      sp.species = i;
      sp.variant = v;
      sp.origin = Origin::Keepsake;
      if (!insertStored(sp)) {
        saveStorage();
        return;
      }
    }
  }
  saveStorage();
}

void Pet::saveStorage() {
  static uint8_t buf[kBoxMax];   // static: ~6.7 KB is too much for the ESP32 loop stack
  size_t n = writeStorage(buf, sizeof(buf));
  if (!n) return;
  if (boxCount_ == 0) kv_.remove(kBoxKey);
  else kv_.putBytes(kBoxKey, buf, n);
}

size_t Pet::writeStorage(uint8_t *buf, size_t cap) const {
  Writer w(buf, cap);
  auto key = [&](uint16_t idx) { return idx < theme_.speciesCount ? theme_.species[idx].key : (uint16_t)0; };
  w.u32(kBoxMagic);
  w.u8(kBoxVersion);
  w.u16(hash16(theme_.id));
  w.u16(boxCount_);
  for (uint8_t i = 0; i < boxCount_; i++) {
    const Specimen &b = box_[i];
    w.u16(key(b.species));
    w.u8((uint8_t)(b.variant | b.favouriteKnown << 1 | b.sleeping << 2 | (uint8_t)b.origin << 3));
    w.u16(key(b.keepsakeOf));
    w.bytes(b.keptFromNick, kNickLen);
    w.bytes(b.nick, kNickLen);
    w.u32(b.ageMinutes);
    w.u8(b.food); w.u8(b.joy); w.u8(b.energy); w.u8(b.hygiene); w.u8(b.mess); w.u8(b.weight);
    w.u8(b.careMistakes); w.u8(b.bond); w.u8(b.bondToday);
    w.u16(b.medals); w.u16(b.evoDeclinedLv); w.u16(b.goodTicks);
    w.u32(b.farDeclinedAge);
  }
  return w.ok() && w.n <= cap ? w.n : 0;
}

bool Pet::loadStorage() {
  static uint8_t buf[kBoxMax];
  size_t len = kv_.getBytes(kBoxKey, buf, sizeof(buf));
  return readStorage(buf, len, true);
}

// apply = false: only check the blob parses (import validates before it replaces anything).
bool Pet::readStorage(const uint8_t *buf, size_t len, bool apply) {
  if (apply) boxCount_ = 0;
  if (len < 9) return len == 0;   // no blob = empty storage
  Reader r(buf, len);
  if (r.u32() != kBoxMagic || r.u8() != kBoxVersion || r.u16() != hash16(theme_.id)) return false;
  uint16_t n = r.u16();
  for (uint16_t i = 0; i < n && !r.bad; i++) {
    Specimen b;
    uint16_t sk = r.u16();
    uint8_t flags = r.u8();
    uint16_t kk = r.u16();
    r.bytes(b.keptFromNick, kNickLen);
    r.bytes(b.nick, kNickLen);
    b.keptFromNick[kNickLen - 1] = b.nick[kNickLen - 1] = 0;
    b.ageMinutes = r.u32();
    b.food = r.u8(); b.joy = r.u8(); b.energy = r.u8(); b.hygiene = r.u8(); b.mess = r.u8(); b.weight = r.u8();
    b.careMistakes = r.u8(); b.bond = r.u8(); b.bondToday = r.u8();
    b.medals = r.u16(); b.evoDeclinedLv = r.u16(); b.goodTicks = r.u16();
    b.farDeclinedAge = r.u32();
    if (r.bad || !apply) continue;
    int16_t si = indexOfKey(sk), ki = kk ? indexOfKey(kk) : -1;
    if (si < 0) continue;                      // species left the roster: dropped
    b.species = (uint16_t)si;
    b.keepsakeOf = ki >= 0 ? (uint16_t)ki : kNoSpecies;
    b.variant = flags & 1;
    b.favouriteKnown = flags & 2;
    b.sleeping = flags & 4;
    b.origin = (flags >> 3) & 1 ? Origin::Keepsake : Origin::Hatched;
    if (findStored(b.species, b.variant) >= 0) continue;   // never two of a kind
    if (!insertStored(b)) break;
  }
  return !r.bad;
}

}  // namespace tama
