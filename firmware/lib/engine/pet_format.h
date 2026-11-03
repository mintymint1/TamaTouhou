#pragma once
// Internal to the Pet implementation (pet*.cpp): save-blob keys, magic numbers and the
// little-endian writer/reader. Not part of the engine's public API.
#include <cstddef>
#include <cstdint>

#include "pet.h"

namespace tama {
namespace detail {

inline constexpr char kSaveKey[] = "pet";
constexpr uint32_t kSaveMagic = 0x54504554;  // "TPET"
// v2 = v1 + active origin/keepsake + UI settings. v3 = v2 + egg choice + stint (store gate),
// and the two colours are separate collections (a v2 variant also set the normal bit).
constexpr uint8_t kSaveVersion = 3;
constexpr size_t kSaveMax = 1400;            // fixed fields + 2 key lists of up to 256

// Storage lives in its own blob, written only when it changes (keepsake, store, switch,
// release): NVS writes freeze the CPU for ~1 s, so it must never ride the periodic save.
inline constexpr char kBoxKey[] = "box";
constexpr uint32_t kBoxMagic = 0x584F4254;   // "TBOX"
constexpr uint8_t kBoxVersion = 1;
constexpr size_t kBoxRecord = 52;            // bytes per stored creature
constexpr size_t kBoxMax = 9 + kStorageMax * kBoxRecord;

// Export package: "TSAV" | u8 version | u16 theme hash | u16 pet len | u16 box len |
// pet blob | box blob | u32 CRC32 of everything before it.
constexpr uint32_t kExportMagic = 0x56415354;  // "TSAV"
constexpr uint8_t kExportVersion = 1;
constexpr size_t kExportHeader = 11;
static_assert(kExportHeader + kSaveMax + kBoxMax + 4 <= Pet::kExportMax, "export buffer too small");

inline uint32_t crc32(const uint8_t *p, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; i++) {
    c ^= p[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
  }
  return ~c;
}

// Little-endian writer/reader so the save layout never depends on struct padding.
struct Writer {
  uint8_t *p;
  size_t n = 0, cap;
  Writer(uint8_t *buf, size_t c) : p(buf), cap(c) {}
  void u8(uint8_t v) { if (n < cap) p[n++] = v; }
  void u16(uint16_t v) { u8(v & 0xFF); u8(v >> 8); }
  void u32(uint32_t v) { u16(v & 0xFFFF); u16(v >> 16); }
  void bytes(const void *b, size_t len) { for (size_t i = 0; i < len; i++) u8(((const uint8_t *)b)[i]); }
  bool ok() const { return n <= cap; }
};

struct Reader {
  const uint8_t *p;
  size_t n = 0, len;
  bool bad = false;
  Reader(const uint8_t *buf, size_t l) : p(buf), len(l) {}
  uint8_t u8() { if (n >= len) { bad = true; return 0; } return p[n++]; }
  uint16_t u16() { uint16_t lo = u8(); return (uint16_t)(lo | (u8() << 8)); }
  uint32_t u32() { uint32_t lo = u16(); return lo | ((uint32_t)u16() << 16); }
  void bytes(void *b, size_t l) { for (size_t i = 0; i < l; i++) ((uint8_t *)b)[i] = u8(); }
};

}  // namespace detail
}  // namespace tama
