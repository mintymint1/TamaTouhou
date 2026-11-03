// Packed sprite reader (lib/engine/sprite_pack.*) against real packs from tools/build_theme.py
// --pack, plus malformed input (the files arrive over USB, so they're untrusted).
#include <cstdio>
#include <vector>

#include "framework.h"
#include "sprite_pack.h"

using namespace tama;

namespace {

std::vector<uint8_t> readFile(const char *path) {
  std::vector<uint8_t> v;
  FILE *f = std::fopen(path, "rb");
  if (!f) return v;
  int c;
  while ((c = std::fgetc(f)) != EOF) v.push_back((uint8_t)c);
  std::fclose(f);
  return v;
}

const char *kPacked[] = {"../../data/sprites/reimu.spr", "../../data/sprites/marisa.spr",
                         "../../data/sprites/kitten.spr"};

// A tiny hand-made pack: 1 colour, idle slot, one 3x2 frame: row0 = [T, O, O], row1 = [O, T, T]
std::vector<uint8_t> tinyPack() {
  std::vector<uint8_t> b = {'S', 'P', 'R', '1', 1, 0, 1, 0, 0xFF, 0xFF, 0, 1};
  // slot: id 0, 1 frame, w 3, h 2, baseline 2, reserved, ms 100
  b.insert(b.end(), {0, 1, 3, 2, 2, 0, 100, 0});
  uint32_t off = (uint32_t)b.size() + 6;
  b.insert(b.end(), {(uint8_t)off, (uint8_t)(off >> 8), 0, 0, 3, 0});   // offset, opaque=3
  b.insert(b.end(), {0x80, 0x01, 0, 0, 0x00, 0, 0x81});                 // T, OO | O, TT
  return b;
}

}  // namespace

TEST(sprite_tiny_pack_decodes) {
  auto b = tinyPack();
  SpritePack sp;
  CHECK(sp.parse(b.data(), b.size()));
  CHECK(sp.has(SLOT_IDLE));
  CHECK(!sp.has(SLOT_WALK));
  int pixels = 0, runs = 0;
  bool ok = sp.decode(SLOT_IDLE, 0, [&](int x, int y, int n, const uint8_t *) { pixels += n; runs++; (void)x; (void)y; });
  CHECK(ok);
  CHECK_EQ(pixels, 3);
  CHECK_EQ(runs, 2);
}

TEST(sprite_real_packs_decode_every_frame) {
  int found = 0;
  for (const char *path : kPacked) {
    auto b = readFile(path);
    if (b.empty()) continue;   // run tools/build_theme.py touhou --pack first
    found++;
    SpritePack sp;
    CHECK(sp.parse(b.data(), b.size()));
    CHECK(sp.paletteCount() > 0);
    for (uint8_t s = 0; s < SLOT_COUNT; s++) {
      if (!sp.has(s)) continue;
      const SpriteSlot &sl = sp.slot(s);
      CHECK(sl.w > 0 && sl.h > 0);
      for (uint8_t f = 0; f < sl.frames; f++) {
        bool inBounds = true;
        bool ok = sp.decode(s, f, [&](int x, int y, int n, const uint8_t *) {
          if (x < 0 || y < 0 || x + n > sl.w || y >= sl.h) inBounds = false;
        });
        CHECK(ok);
        CHECK(inBounds);
      }
    }
  }
  if (!found) std::printf("    (no packed sprites found: run tools/build_theme.py touhou --pack)\n");
}

TEST(sprite_truncated_files_are_rejected_without_crashing) {
  auto full = readFile(kPacked[0]);
  if (full.empty()) full = tinyPack();
  for (size_t len = 0; len < full.size(); len += (full.size() > 4000 ? 7 : 1)) {
    std::vector<uint8_t> cut(full.begin(), full.begin() + len);
    SpritePack sp;
    if (!sp.parse(cut.data(), cut.size())) continue;
    for (uint8_t s = 0; s < SLOT_COUNT; s++)
      for (uint8_t f = 0; sp.has(s) && f < sp.slot(s).frames; f++)
        sp.decode(s, f, [](int, int, int, const uint8_t *) {});   // must not read past `cut`
  }
  CHECK(true);
}

TEST(sprite_corrupt_bytes_never_overrun) {
  auto base = readFile(kPacked[0]);
  if (base.empty()) base = tinyPack();
  uint32_t seed = 99;
  for (int trial = 0; trial < 400; trial++) {
    auto b = base;
    for (int k = 0; k < 4; k++) {
      seed = seed * 1664525u + 1013904223u;
      b[seed % b.size()] = (uint8_t)(seed >> 24);
    }
    SpritePack sp;
    if (!sp.parse(b.data(), b.size())) continue;
    for (uint8_t s = 0; s < SLOT_COUNT; s++)
      for (uint8_t f = 0; sp.has(s) && f < sp.slot(s).frames; f++)
        sp.decode(s, f, [&](int x, int, int n, const uint8_t *idx) {
          for (int i = 0; i < n; i++) CHECK(idx[i] < sp.paletteCount());
          (void)x;
        });
  }
}

TEST(sprite_frame_timing) {
  auto b = tinyPack();
  SpritePack sp;
  CHECK(sp.parse(b.data(), b.size()));
  CHECK_EQ(sp.frameAt(SLOT_IDLE, 0), 0);
  CHECK_EQ(sp.frameAt(SLOT_IDLE, 250), 0);   // single frame loops on itself
}

TEST(sprite_rejects_bad_headers) {
  auto b = tinyPack();
  SpritePack sp;
  auto bad = b; bad[0] = 'X';
  CHECK(!sp.parse(bad.data(), bad.size()));
  bad = b; bad[4] = 2;                        // unknown version
  CHECK(!sp.parse(bad.data(), bad.size()));
  bad = b; bad[6] = 0;                        // zero colours
  CHECK(!sp.parse(bad.data(), bad.size()));
  bad = b; bad[12] = 1;                       // walk only, no idle
  CHECK(!sp.parse(bad.data(), bad.size()));
}

TEST(sprite_pingpong_playback) {
  // 4 frames, 100 ms, ping-pong: 0 1 2 3 2 1 0 1 ...
  std::vector<uint8_t> b = {'S', 'P', 'R', '1', 1, 0, 1, 0, 0xFF, 0xFF, 0, 1};
  b.insert(b.end(), {0, 4, 1, 1, 1, 1, 100, 0});   // flags = 1
  uint32_t off = (uint32_t)b.size() + 4 * 6;
  for (int f = 0; f < 4; f++) b.insert(b.end(), {(uint8_t)(off + f * 2), (uint8_t)((off + f * 2) >> 8), 0, 0, 1, 0});
  for (int f = 0; f < 4; f++) b.insert(b.end(), {0x00, 0});
  SpritePack sp;
  CHECK(sp.parse(b.data(), b.size()));
  CHECK(sp.slot(SLOT_IDLE).pingpong);
  const uint8_t want[8] = {0, 1, 2, 3, 2, 1, 0, 1};
  for (int i = 0; i < 8; i++) CHECK_EQ(sp.frameAt(SLOT_IDLE, i * 100 + 50), want[i]);
}
