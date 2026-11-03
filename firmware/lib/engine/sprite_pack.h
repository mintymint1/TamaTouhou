#pragma once
// Reader for packed sprites (.spr), written by tools/build_theme.py pack. Portable (no
// Arduino), so it's host-tested; the firmware supplies a draw callback.
//
// Format "SPR1", little-endian:
//   char[4] "SPR1" | u16 version (1) | u16 palCount (<= 255) | u16 pal[palCount] (RGB565)
//   u8 hasVariant | [u16 palVariant[palCount]]   (rare variant = same pixels, other palette)
//   u8 slotCount
//   per slot: u8 slot | u8 frames | u8 w | u8 h | u8 baseline | u8 flags | u16 frameMs
//             flags bit 0 = ping-pong playback (0,1,2,3,2,1,...): pose-holding loops like
//             sleep/sad don't snap back from the last frame to the first
//             then per frame: u32 offset (from file start) | u16 opaqueCount
//   frame data: rows top to bottom; each row is a run list until w pixels are covered:
//             c & 0x80 -> (c & 0x7F) + 1 transparent pixels
//             else     -> c + 1 opaque pixels, followed by that many palette indices
// The file is untrusted (it arrives over USB): parse() checks every offset, and decode()
// checks each run stays inside the frame and the data, and the opaque count matches.
#include <cstddef>
#include <cstdint>

namespace tama {

enum SpriteSlotId : uint8_t {
  SLOT_IDLE = 0, SLOT_WALK, SLOT_SLEEP, SLOT_EAT, SLOT_SAD, SLOT_HAPPY,
  SLOT_FLAIR1, SLOT_FLAIR2, SLOT_FLAIR3, SLOT_COUNT
};

struct SpriteSlot {
  uint8_t frames = 0, w = 0, h = 0, baseline = 0;   // baseline = rows down to the foot line
  bool pingpong = false;
  uint16_t frameMs = 100;
  static constexpr uint8_t kMaxFrames = 24;
  uint32_t offset[kMaxFrames] = {0};
  uint16_t opaque[kMaxFrames] = {0};
};

class SpritePack {
 public:
  bool parse(const uint8_t *data, size_t len);
  bool has(uint8_t slot) const { return slot < SLOT_COUNT && slots_[slot].frames > 0; }
  const SpriteSlot &slot(uint8_t s) const { return slots_[s < SLOT_COUNT ? s : 0]; }
  const uint16_t *palette(bool variant) const { return variant && hasVariant_ ? palVar_ : pal_; }
  bool hasVariant() const { return hasVariant_; }
  uint16_t paletteCount() const { return palCount_; }

  // Frame to show `elapsedMs` into the slot's animation.
  uint8_t frameAt(uint8_t slot, uint32_t elapsedMs, bool loop = true) const;

  // Calls run(x, y, len, indices) for every opaque run of a frame. Returns false (and stops)
  // on malformed data.
  template <class F>
  bool decode(uint8_t slotId, uint8_t frame, F run) const {
    const SpriteSlot &s = slot(slotId);
    if (frame >= s.frames) return false;
    const uint8_t *p = data_ + s.offset[frame];
    const uint8_t *end = data_ + len_;
    uint32_t opaque = 0;
    for (uint8_t y = 0; y < s.h; y++) {
      uint16_t x = 0;
      while (x < s.w) {
        if (p >= end) return false;
        uint8_t c = *p++;
        uint16_t n = (uint16_t)((c & 0x7F) + 1);
        if (x + n > s.w) return false;
        if (!(c & 0x80)) {
          if (p + n > end) return false;
          for (uint16_t i = 0; i < n; i++)
            if (p[i] >= palCount_) return false;
          run((int)x, (int)y, (int)n, p);
          p += n;
          opaque += n;
        }
        x = (uint16_t)(x + n);
      }
    }
    return opaque == s.opaque[frame];
  }

 private:
  const uint8_t *data_ = nullptr;
  size_t len_ = 0;
  uint16_t palCount_ = 0;
  uint16_t pal_[256] = {0}, palVar_[256] = {0};
  bool hasVariant_ = false;
  SpriteSlot slots_[SLOT_COUNT];
};

}  // namespace tama
