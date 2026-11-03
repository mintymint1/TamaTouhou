// Save export/import (Pet::exportSave / importSave + save_text) and the v2 -> v3 migration.
#include <string>
#include <vector>

#include "framework.h"
#include "kv_mem.h"
#include "pet.h"
#include "save_text.h"
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

uint32_t crc(const uint8_t *p, size_t n) {   // same CRC32 as the engine
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; i++) {
    c ^= p[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
  }
  return ~c;
}

void putCrc(std::vector<uint8_t> &pkg) {
  uint32_t c = crc(pkg.data(), pkg.size() - 4);
  for (int i = 0; i < 4; i++) pkg[pkg.size() - 4 + i] = (uint8_t)(c >> (8 * i));
}

struct World {
  MemKv kv;
  Rng rng{99};
  Config cfg;
  uint32_t now = 1000;
  Pet pet{kTheme, kv, rng, cfg};

  // a game with some history: an evolved, nicknamed pet, a keepsake, a rare-colour entry
  void play() {
    pet.begin(now);
    pet.chooseStarter(0);
    pet.eggTap(now); pet.eggTap(now); pet.eggTap(now);
    if (pet.variant()) pet.debugToggleVariant();
    pet.debugSetSpecies(sp("kitten"));
    pet.debugSetNeeds(100, 100, 100, 100);
    pet.debugSetLevel(18);
    pet.evolve(now);
    pet.rename("MIKO");
    pet.debugCollect(sp("reimu"), true);
    pet.advanceMinutes(30);
  }
  std::vector<uint8_t> exported() {
    std::vector<uint8_t> buf(Pet::kExportMax);
    size_t n = pet.exportSave(buf.data(), buf.size());
    buf.resize(n);
    return buf;
  }
};

}  // namespace

TEST(export_then_import_restores_the_whole_game) {
  World a;
  a.play();
  std::vector<uint8_t> pkg = a.exported();
  CHECK(pkg.size() > 100);

  World b;                             // another device with its own new game
  b.pet.begin(b.now);
  b.pet.takeEvents();
  CHECK(b.pet.importSave(pkg.data(), pkg.size(), b.now) == ImportResult::Ok);
  CHECK(b.pet.takeEvents() & EV_LOADED);
  CHECK_EQ(b.pet.species(), a.pet.species());
  CHECK(std::strcmp(b.pet.nick(), "MIKO") == 0);
  CHECK_EQ(b.pet.level(), a.pet.level());
  CHECK_EQ(b.pet.storageCount(), a.pet.storageCount());
  CHECK_EQ(b.pet.stored(0).species, sp("kitten"));
  CHECK_EQ(b.pet.collectedCount(), a.pet.collectedCount());
  CHECK(b.pet.isVariantCollected(sp("reimu")));
  CHECK_EQ(b.pet.stintMinutes(), a.pet.stintMinutes());
  CHECK(b.pet.awakenedThisStint());

  Pet c(kTheme, b.kv, b.rng, b.cfg);   // and it was persisted: a reboot keeps it
  c.begin(b.now);
  CHECK_EQ(c.species(), a.pet.species());
  CHECK_EQ(c.storageCount(), a.pet.storageCount());
}

TEST(save_text_round_trips_in_both_layouts) {
  World a;
  a.play();
  std::vector<uint8_t> pkg = a.exported();
  for (bool oneLine : {true, false}) {
    std::vector<char> txt(saveTextCapacity(pkg.size(), kTheme.id));
    size_t n = encodeSaveText(pkg.data(), pkg.size(), kTheme.id, txt.data(), txt.size(), oneLine);
    CHECK(n > 0);
    std::string s(txt.data(), n);
    CHECK(s.rfind("TAMA-SAVE 1 ", 0) == 0);
    CHECK_EQ(s.find('\n') == std::string::npos || !oneLine, true);
    std::vector<uint8_t> back(Pet::kExportMax);
    char theme[32];
    size_t m = decodeSaveText(s.c_str(), s.size(), back.data(), back.size(), theme, sizeof(theme));
    CHECK_EQ(m, pkg.size());
    CHECK(std::memcmp(back.data(), pkg.data(), pkg.size()) == 0);
    CHECK(std::strcmp(theme, kTheme.id) == 0);
  }
  uint8_t junk[16];
  char theme[8];
  CHECK_EQ(decodeSaveText("hello", 5, junk, sizeof(junk), theme, sizeof(theme)), 0u);
  CHECK_EQ(decodeSaveText("TAMA-SAVE 9 x QUJD", 18, junk, sizeof(junk), theme, sizeof(theme)), 0u);
}

TEST(import_refuses_bad_packages_and_changes_nothing) {
  World a;
  a.play();
  std::vector<uint8_t> pkg = a.exported();

  World b;
  b.pet.begin(b.now);
  b.pet.chooseStarter(1);
  uint16_t before = b.pet.species();
  int writes = b.kv.writes;

  std::vector<uint8_t> bad = pkg;
  bad[bad.size() / 2] ^= 0x55;
  CHECK(b.pet.importSave(bad.data(), bad.size(), b.now) == ImportResult::Corrupt);
  CHECK(b.pet.importSave(pkg.data(), pkg.size() - 7, b.now) == ImportResult::BadFormat);
  const uint8_t garbage[20] = {1, 2, 3};
  CHECK(b.pet.importSave(garbage, sizeof(garbage), b.now) == ImportResult::BadFormat);
  std::vector<uint8_t> other = pkg;    // a package from another theme (valid CRC)
  other[5] ^= 0xFF;
  putCrc(other);
  CHECK(b.pet.importSave(other.data(), other.size(), b.now) == ImportResult::WrongTheme);
  CHECK_EQ(b.pet.species(), before);
  CHECK(b.pet.awaitingStarter() == false);
  CHECK_EQ(b.kv.writes, writes);       // nothing was written
}

TEST(v2_saves_migrate_stint_and_awakening) {
  World a;
  a.play();                            // an evolved pet, hatched in your care, Lv 18+
  std::vector<uint8_t> pkg = a.exported();
  size_t petLen = pkg[7] | pkg[8] << 8;
  std::vector<uint8_t> v2(pkg.begin() + 11, pkg.begin() + 11 + petLen - 5);   // drop the v3 tail
  v2[4] = 2;                           // version byte
  MemKv kv;
  kv.putBytes("pet", v2.data(), v2.size());
  Rng rng{1};
  Pet old(kTheme, kv, rng);
  old.begin(a.now);
  CHECK_EQ(old.species(), a.pet.species());
  CHECK_EQ(old.stintMinutes(), old.ageMinutes());   // raised from its egg: its whole life counts
  CHECK(old.awakenedThisStint());                   // an evolved form that hatched in your care
  CHECK(old.canStoreActive() == StoreError::Ok);
}
