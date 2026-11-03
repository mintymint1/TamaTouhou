#pragma once
// ES8311 codec -> NS4150B amplifier -> speaker. Square-wave "Game Boy" effects synthesised
// on a FreeRTOS task (core 0) so the game loop never blocks. Ported from TamaPoke (MIT).
#include <cstdint>

namespace hw {

enum Sfx : uint8_t {
  SFX_TAP = 0, SFX_EAT, SFX_PLAY, SFX_HEART, SFX_HATCH, SFX_EVOLVE, SFX_MEDAL, SFX_DENY,
  SFX_BYE, SFX_LEVEL, SFX_COUNT
};

void audioBegin();               // I2S + codec + amp + task; plays the boot jingle
void sfx(uint8_t id);            // queue an effect (dropped if the queue is full)
void audioSetEnabled(bool on);   // not persisted: the pet's save holds the setting
bool audioEnabled();
void audioSetSleeping(bool s);   // amp off while the pet sleeps

}  // namespace hw
