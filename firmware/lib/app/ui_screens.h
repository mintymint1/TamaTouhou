#pragma once
// The hub screens (docs/design.md). ui.cpp routes gestures and rendering
// to these; each lives in its own file.
#include <cstdint>

#include "nav.h"

namespace ui {

// Status (swipe up): 5 pages: Next up / Care / Growth / Bond & streak / Collection
uint8_t statusPages();
void statusRender(uint32_t now);
void statusTap(int x, int y, uint32_t now);

// Chronicle (swipe right): every roster entry, seen or not, with a detail view
uint8_t chroniclePages();
void chronicleEnter();
void chronicleRender(uint32_t now);
void chronicleTap(int x, int y, uint32_t now);
bool chronicleSwipe(nav::Swipe s);   // true = handled inside (closes the detail view)
bool chronicleBack();                // back mark: true = closed the detail view
void chronicleLongPress(int x, int y, uint32_t now);   // hold to switch with the stored copy

// Storage (swipe left): the creatures kept in stasis; switch / store / release
uint8_t storagePages();
void storageEnter();
void storageRender(uint32_t now);
void storageTap(int x, int y, uint32_t now);
void storageLongPress(int x, int y, uint32_t now);
bool storageSwipe(nav::Swipe s);
bool storageBack();
bool storageBusy(uint32_t now);      // a dialog or the switch effect is showing
void storageOpen(uint16_t species, bool variant);   // jump straight to a stored creature's detail
// Open that stored creature's detail with the switch dialog up (Chronicle "hold to switch").
// Returns false (and shows why) when it can't be switched in.
bool storageAskSwitch(uint16_t species, bool variant, uint32_t now);

// Settings (swipe down): clock, sound, brightness, bezel arc
void settingsRender(uint32_t now);
void settingsTap(int x, int y, uint32_t now);

// Main-screen extras
void drawBezelArc();                 // progress of the nearest milestone around the bezel
void drawEdgeHints(uint16_t col);    // first-run arrows until each screen has been opened

}  // namespace ui
