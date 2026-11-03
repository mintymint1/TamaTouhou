// plat::* for the board: art from LittleFS into PSRAM, sound through hw_audio, the RTC clock.
// The PC version is firmware/sim/platform_pc.cpp.
#include "platform_esp32.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <esp_random.h>

#include <cstdarg>

#include "app_platform.h"
#include "gfx.h"
#include "hw_audio.h"
#include "hw_display.h"
#include "hw_power.h"

Gfx *app::gfx = nullptr;   // set to hw::gfx in setup(), after the display starts

static uint8_t gBrightness = 0;

bool assetsBegin() {
  // Never format here: a failed mount means the art wasn't flashed (make flash). The game
  // still runs, just without art, rather than wiping anything.
  if (!LittleFS.begin(false)) {
    Serial.println("LittleFS mount failed: flash the art with `make flash`");
    return false;
  }
  return true;
}

uint8_t platformBrightnessLevel() { return gBrightness; }

// Rotation belongs to the board, not the pet: its own NVS namespace, outside the per-theme
// save, so importing or switching pets never turns the screen.
static uint8_t gRotation = 0;
static bool gRotationDirty = false;
static constexpr char kDeviceNs[] = "device", kRotationKey[] = "rot";

void rotationBegin() {
  Preferences p;
  if (p.begin(kDeviceNs, true)) {   // read-only; fails harmlessly before the first store
    gRotation = p.getUChar(kRotationKey, 0) & 3;
    p.end();
  }
  hw::gfx->setRotation(gRotation);
}

void rotationFlush() {
  if (!gRotationDirty) return;
  gRotationDirty = false;
  Preferences p;
  if (p.begin(kDeviceNs, false)) {
    p.putUChar(kRotationKey, gRotation);
    p.end();
  }
}

namespace plat {

void sfx(uint8_t id) { hw::sfx(id); }   // same ids (plat::Sfx == hw::Sfx)

uint32_t random32() { return esp_random(); }

bool readFile(const char *path, uint8_t **data, size_t *len, size_t maxLen) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  size_t n = f.size();
  if (n == 0 || n > maxLen) {
    f.close();
    return false;
  }
  uint8_t *p = (uint8_t *)ps_malloc(n);   // PSRAM: sprite files are up to a few hundred KB
  if (!p) p = (uint8_t *)malloc(n);
  bool ok = p && f.read(p, n) == n;
  f.close();
  if (!ok) {
    free(p);
    return false;
  }
  *data = p;
  *len = n;
  return true;
}

void freeFile(uint8_t *data) { free(data); }

void log(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.print(buf);
}

Power power() {   // hw:: caches the AXP2101 reads for ~2 s
  Power p;
  int pct = hw::batteryPercent();
  p.percent = (int8_t)(pct < 0 ? -1 : pct > 100 ? 100 : pct);
  p.charging = pct >= 0 && hw::batteryCharging();
  p.external = hw::usbPresent();
  return p;
}

uint32_t epoch() { return hw::rtcEpoch(); }
void setEpoch(uint32_t e) { hw::rtcSetEpoch(e); }
void setSoundEnabled(bool on) { hw::audioSetEnabled(on); }
void setBrightnessLevel(uint8_t b) { gBrightness = b < 4 ? b : 0; }
uint8_t rotation() { return gRotation; }
void setRotation(uint8_t q) {
  gRotation = q & 3;
  hw::gfx->setRotation(gRotation);   // Arduino_Canvas turns everything drawn from now on
  gRotationDirty = true;              // stored by rotationFlush() once the taps stop
}

}  // namespace plat
