#pragma once
// Square-wave "Game Boy" effects (from TamaPoke, MIT). Shared by the board (hw_audio.cpp,
// ES8311 over I2S) and the PC simulator (SDL audio), so both sound the same.
#include <cstdint>

#include "app_platform.h"

namespace sfxdata {

struct Note { uint16_t f, ms; };   // f = 0 is a rest

static const Note N_TAP[] = {{880, 35}};
static const Note N_EAT[] = {{660, 45}, {0, 12}, {660, 45}};
static const Note N_PLAY[] = {{784, 45}, {988, 60}};
static const Note N_HEART[] = {{1047, 55}, {1319, 90}};
static const Note N_HATCH[] = {{523, 80}, {659, 80}, {784, 110}, {1047, 170}};
static const Note N_EVOLVE[] = {{523, 80}, {659, 80}, {784, 80}, {1047, 90}, {1319, 230}};
static const Note N_MEDAL[] = {{784, 70}, {0, 25}, {784, 70}, {0, 25}, {1047, 200}};
static const Note N_DENY[] = {{300, 110}, {200, 170}};
static const Note N_BYE[] = {{784, 150}, {659, 150}, {523, 280}};
static const Note N_LEVEL[] = {{784, 70}, {1047, 130}};

struct Def { const Note *n; uint8_t len; };
static const Def kSfx[plat::SFX_COUNT] = {
    {N_TAP, 1}, {N_EAT, 3}, {N_PLAY, 2}, {N_HEART, 2}, {N_HATCH, 4},
    {N_EVOLVE, 5}, {N_MEDAL, 5}, {N_DENY, 2}, {N_BYE, 3}, {N_LEVEL, 2},
};

// Fill `out` (mono, 16-bit) with one note: square wave with an anti-click attack/decay ramp.
inline int renderNote(const Note &n, int sampleRate, int16_t *out, int maxSamples, int16_t amp = 5000) {
  int total = sampleRate * n.ms / 1000;
  if (total > maxSamples) total = maxSamples;
  int half = n.f ? sampleRate / (2 * n.f) : 0;
  int phase = 0;
  bool high = true;
  for (int i = 0; i < total; i++) {
    int16_t s = 0;
    if (n.f) {
      s = high ? amp : (int16_t)-amp;
      if (i < 64) s = (int16_t)(s * i / 64);
      else if (i > total - 96) s = (int16_t)(s * (total - i) / 96);
      if (++phase >= half) { phase = 0; high = !high; }
    }
    out[i] = s;
  }
  return total;
}

}  // namespace sfxdata
