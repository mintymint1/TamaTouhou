#pragma once
// Theme art: /sprites/<species id>.spr, /items/<name>.spr, /bg/<habitat id>.spr.
// On the board they live on LittleFS (firmware/data/ via `pio run -t uploadfs`) and are read
// into PSRAM; on the PC they're read from firmware/data/. Parsed with tama::SpritePack.
#include <cstddef>
#include <cstdint>

#include "sprite_pack.h"

struct Asset {
  uint8_t *data = nullptr;
  size_t len = 0;
  tama::SpritePack pack;
  bool ok = false;
  char path[40] = {0};

  bool load(const char *path);   // no-op if already loaded from the same path
  void unload();
};

// Draw one frame. (cx, groundY) = bottom-centre of the content; `scale` = integer zoom.
// `shade`: 255 = normal colours, lower = darker (night, sleep, sad). `silhouette` != 0 paints
// every pixel that colour (evolution flicker, undiscovered entries).
void drawSprite(const tama::SpritePack &sp, uint8_t slot, uint8_t frame, int cx, int groundY,
                int scale, bool variant = false, bool mirror = false, uint8_t shade = 255,
                uint16_t silhouette = 0);

// Integer zoom so the idle content is ~target px tall (1..5), as in TamaPoke.
int spriteScale(const tama::SpritePack &sp, int targetPx = 170);

uint16_t shade565(uint16_t c, uint8_t f);   // scale RGB565 brightness by f/255
