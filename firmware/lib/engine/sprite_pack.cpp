#include "sprite_pack.h"

#include <cstring>

namespace tama {

namespace {
uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t rd32(const uint8_t *p) { return (uint32_t)rd16(p) | ((uint32_t)rd16(p + 2) << 16); }
}  // namespace

bool SpritePack::parse(const uint8_t *data, size_t len) {
  *this = SpritePack();
  if (!data || len < 9 || std::memcmp(data, "SPR1", 4) != 0 || rd16(data + 4) != 1) return false;
  size_t o = 6;
  palCount_ = rd16(data + o);
  o += 2;
  if (palCount_ == 0 || palCount_ > 255 || o + palCount_ * 2u + 1 > len) return false;
  for (uint16_t i = 0; i < palCount_; i++) pal_[i] = rd16(data + o + i * 2);
  o += palCount_ * 2u;
  hasVariant_ = data[o++] != 0;
  if (hasVariant_) {
    if (o + palCount_ * 2u > len) return false;
    for (uint16_t i = 0; i < palCount_; i++) palVar_[i] = rd16(data + o + i * 2);
    o += palCount_ * 2u;
  }
  if (o + 1 > len) return false;
  uint8_t count = data[o++];
  for (uint8_t k = 0; k < count; k++) {
    if (o + 8 > len) return false;
    uint8_t id = data[o], frames = data[o + 1];
    SpriteSlot s;
    s.frames = frames;
    s.w = data[o + 2];
    s.h = data[o + 3];
    s.baseline = data[o + 4];
    s.pingpong = data[o + 5] & 1;
    s.frameMs = rd16(data + o + 6);
    o += 8;
    if (id >= SLOT_COUNT || frames == 0 || frames > SpriteSlot::kMaxFrames || !s.w || !s.h ||
        s.baseline > s.h || o + frames * 6u > len)
      return false;
    if (s.frameMs == 0) s.frameMs = 100;   // TamaPoke 1.10: a 0 duration hung frameAt()
    for (uint8_t f = 0; f < frames; f++) {
      s.offset[f] = rd32(data + o);
      s.opaque[f] = rd16(data + o + 4);
      o += 6;
      if (s.offset[f] >= len) return false;
    }
    slots_[id] = s;
  }
  data_ = data;
  len_ = len;
  return has(SLOT_IDLE);   // idle is required (docs/themes.md)
}

uint8_t SpritePack::frameAt(uint8_t slotId, uint32_t elapsedMs, bool loop) const {
  const SpriteSlot &s = slot(slotId);
  if (!s.frames) return 0;
  uint32_t total = (uint32_t)s.frames * s.frameMs;
  if (!loop && elapsedMs >= total) return (uint8_t)(s.frames - 1);
  if (s.pingpong && s.frames > 2) {
    uint32_t period = 2u * s.frames - 2, pos = (elapsedMs / s.frameMs) % period;
    return (uint8_t)(pos < s.frames ? pos : period - pos);
  }
  return (uint8_t)((elapsedMs % total) / s.frameMs);
}

}  // namespace tama
