#pragma once
// Screens for the round 466x466 display. One active screen at a time plus modal overlays
// (food picker, yes/no dialogs), instead of TamaPoke's dozen global xxxOpen flags.
// Everything here reads theme words/colours from lib/theme_gen (never hard-coded).
#include <cstdint>

#include "gesture.h"
#include "pet.h"

namespace ui {

void begin(tama::Pet &pet);
void onGesture(const tama::Gesture &g, uint32_t now);
void render(uint32_t now);           // full repaint + flush
void onEvents(uint32_t events);      // engine events -> sounds / celebrations
bool busyAnimating(uint32_t now);    // true while something on screen should keep it awake
uint32_t frameIntervalMs();          // 100 ms normally, 85 ms in the minigame
uint32_t longPressMs();              // 1 s on the Chronicle/Gap grids, 3 s elsewhere (pet release stays deliberate)

}  // namespace ui
