#pragma once
// What the shared app layer (screens, art loading) needs from whatever it runs on.
// Implemented by src/platform_esp32.cpp on the board and sim/platform_pc.cpp on the PC.
#include <cstddef>
#include <cstdint>

namespace plat {

enum Sfx : uint8_t {
  SFX_TAP = 0, SFX_EAT, SFX_PLAY, SFX_HEART, SFX_HATCH, SFX_EVOLVE, SFX_MEDAL, SFX_DENY,
  SFX_BYE, SFX_LEVEL, SFX_COUNT
};

void sfx(uint8_t id);                       // queue a sound effect (never blocks)
uint32_t random32();
inline uint32_t rnd(uint32_t n) { return n ? random32() % n : 0; }

// Read a whole asset file (paths like "/sprites/reimu.spr"). Allocates *data (PSRAM on the
// board); release with freeFile(). Files larger than maxLen are refused (untrusted input).
bool readFile(const char *path, uint8_t **data, size_t *len, size_t maxLen);
void freeFile(uint8_t *data);

void log(const char *fmt, ...);

// Battery, shown at the top of Settings. percent: 0-100, -1 = no battery. charging: being
// charged right now. external: USB/charger power is connected (with a full battery that's
// "full", with no battery "USB power"). Board: the AXP2101; PC: the computer's own battery.
struct Power {
  int8_t percent = -1;
  bool charging = false, external = false;
};
Power power();                       // cheap: implementations cache it for ~2 s

// Settings screen hooks
uint32_t epoch();                    // wall clock (RTC on the board), unix seconds
void setEpoch(uint32_t e);           // set the clock (RTC on the board)
void setSoundEnabled(bool on);
void setBrightnessLevel(uint8_t b);  // 0 auto, 1 low, 2 mid, 3 high

// Screen rotation, for a board stood another way up: quarter turns clockwise, 0-3. A device
// setting, not part of the pet's save (importing a pet never turns the screen). The canvas
// turns what it draws, and touches come back through glassToScreen() (gfx.h).
uint8_t rotation();
void setRotation(uint8_t quarterTurns);   // applies at once (the board stores it a moment later)

}  // namespace plat
