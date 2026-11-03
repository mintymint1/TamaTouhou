#include "assets.h"

#include <cstring>

#include "app_platform.h"
#include "gfx.h"

using app::gfx;

bool Asset::load(const char *p) {
  if (ok && strcmp(path, p) == 0) return true;
  unload();
  // bound the allocation: the file is untrusted (it can arrive over USB)
  if (!plat::readFile(p, &data, &len, 2 * 1024 * 1024)) {
    data = nullptr;
    len = 0;
    return false;
  }
  ok = pack.parse(data, len);
  if (!ok) {
    plat::log("bad sprite file %s\n", p);
    unload();
    return false;
  }
  strncpy(path, p, sizeof(path) - 1);
  return true;
}

void Asset::unload() {
  if (data) plat::freeFile(data);
  data = nullptr;
  len = 0;
  ok = false;
  path[0] = 0;
}

uint16_t shade565(uint16_t c, uint8_t f) {
  if (f == 255) return c;
  uint16_t r = (c >> 11) * f / 255, g = ((c >> 5) & 63) * f / 255, b = (c & 31) * f / 255;
  return (uint16_t)(r << 11 | g << 5 | b);
}

int spriteScale(const tama::SpritePack &sp, int targetPx) {
  int h = sp.slot(tama::SLOT_IDLE).h;
  int s = h ? (targetPx + h / 2) / h : 3;
  return s < 1 ? 1 : (s > 5 ? 5 : s);
}

void drawSprite(const tama::SpritePack &sp, uint8_t slot, uint8_t frame, int cx, int groundY,
                int scale, bool variant, bool mirror, uint8_t shade, uint16_t silhouette) {
  if (!sp.has(slot)) slot = tama::SLOT_IDLE;
  const tama::SpriteSlot &s = sp.slot(slot);
  const uint16_t *pal = sp.palette(variant);
  int x0 = cx - s.w * scale / 2;
  int y0 = groundY - s.baseline * scale;
  sp.decode(slot, frame, [&](int x, int y, int n, const uint8_t *idx) {
    // merge neighbouring pixels of the same colour into one fillRect
    int i = 0;
    while (i < n) {
      int j = i + 1;
      while (j < n && idx[j] == idx[i]) j++;
      uint16_t col = silhouette ? silhouette : shade565(pal[idx[i]], shade);
      int px = mirror ? (s.w - (x + j)) : (x + i);
      gfx->fillRect(x0 + px * scale, y0 + y * scale, (j - i) * scale, scale, col);
      i = j;
    }
  });
}
