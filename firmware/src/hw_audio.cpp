#include "hw_audio.h"

#include <Arduino.h>
#include <ESP_I2S.h>
#include <Wire.h>

#include "board.h"

namespace hw {

#define ES8311_ADDR 0x18
#define SAMPLE_RATE 16000

static I2SClass i2s;
static bool ready = false, enabled = true, asleep = false;
static QueueHandle_t queue = nullptr;

// The NS4150B needs far longer than 8 ms to settle after power-up, so switching it on per
// effect swallowed the short ones (TamaPoke 1.6). Keep it on while sound is enabled and
// the pet is awake.
static void updateAmp() { digitalWrite(PA_ENABLE, (ready && enabled && !asleep) ? HIGH : LOW); }

static bool esW(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static uint8_t esR(uint8_t reg) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ES8311_ADDR, 1);
  return Wire.available() ? Wire.read() : 0;
}

// Init verified on this board (TamaPoke, from PlaneRadar2.0): clock derived from BCLK
// (reg01 = 0xBF, no external MCLK) and the internal reference feeding the DAC (reg44 = 0x58).
// Without those two the codec ACKs on I2C but stays silent. 16 kHz, 16-bit, I2S slave.
static bool es8311Init() {
  Wire.beginTransmission(ES8311_ADDR);
  if (Wire.endTransmission() != 0) return false;
  esW(0x0D, 0xFA); esW(0x44, 0x08); esW(0x44, 0x08);
  esW(0x01, 0x30); esW(0x02, 0x00); esW(0x03, 0x10); esW(0x16, 0x24);
  esW(0x04, 0x10); esW(0x05, 0x00); esW(0x0B, 0x00); esW(0x0C, 0x00);
  esW(0x10, 0x1F); esW(0x11, 0x7F);
  esW(0x00, 0x80); esW(0x00, 0x80);
  esW(0x01, 0xBF);
  { uint8_t r = esR(0x06); r &= ~0x20; esW(0x06, r); }
  esW(0x13, 0x10); esW(0x1B, 0x0A); esW(0x1C, 0x6A);
  esW(0x44, 0x58);
  esW(0x02, 0x18); esW(0x05, 0x00); esW(0x03, 0x10); esW(0x04, 0x20);
  { uint8_t r = esR(0x07); r &= 0xC0; esW(0x07, r); }
  esW(0x08, 0xFF);
  { uint8_t r = esR(0x06); r &= 0xE0; r |= 0x03; esW(0x06, r); }
  esW(0x09, 0x0C); esW(0x0A, 0x0C);
  esW(0x00, 0x80); esW(0x01, 0xBF); esW(0x09, 0x0C); esW(0x0A, 0x0C);
  esW(0x17, 0xBF); esW(0x0E, 0x02); esW(0x12, 0x00); esW(0x14, 0x1A);
  esW(0x0D, 0x01); esW(0x15, 0x40); esW(0x37, 0x08); esW(0x45, 0x00);
  esW(0x32, 0xBF);
  { uint8_t r = esR(0x31); r &= 0x9F; esW(0x31, r); }
  return true;
}

struct Note { uint16_t f, ms; };
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
struct SfxDef { const Note *n; uint8_t len; };
static const SfxDef SFX[SFX_COUNT] = {
    {N_TAP, 1}, {N_EAT, 3}, {N_PLAY, 2}, {N_HEART, 2}, {N_HATCH, 4},
    {N_EVOLVE, 5}, {N_MEDAL, 5}, {N_DENY, 2}, {N_BYE, 3}, {N_LEVEL, 2},
};

static int16_t buf[256 * 2];

static void playTone(uint16_t f, uint16_t ms) {   // square wave with an anti-click ramp
  int total = SAMPLE_RATE * ms / 1000;
  int half = f ? SAMPLE_RATE / (2 * f) : 0;
  const int16_t amp = 5000;
  int phase = 0, done = 0;
  bool high = true;
  while (done < total) {
    int n = total - done;
    if (n > 256) n = 256;
    for (int i = 0; i < n; i++) {
      int16_t s = 0;
      if (f) {
        s = high ? amp : -amp;
        int idx = done + i;
        if (idx < 64) s = (int16_t)(s * idx / 64);
        else if (idx > total - 96) s = (int16_t)(s * (total - idx) / 96);
        if (++phase >= half) { phase = 0; high = !high; }
      }
      buf[i * 2] = s;
      buf[i * 2 + 1] = s;
    }
    i2s.write((uint8_t *)buf, n * 4);
    done += n;
  }
}

static void audioTask(void *) {
  uint8_t id;
  for (;;) {
    if (xQueueReceive(queue, &id, portMAX_DELAY) && enabled && !asleep && ready && id < SFX_COUNT)
      for (uint8_t i = 0; i < SFX[id].len; i++) playTone(SFX[id].n[i].f, SFX[id].n[i].ms);
  }
}

void audioBegin() {
  pinMode(PA_ENABLE, OUTPUT);
  digitalWrite(PA_ENABLE, LOW);
  i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("I2S begin failed");
    return;
  }
  if (!es8311Init()) {
    Serial.println("ES8311 not responding (sound off)");
    return;
  }
  ready = true;
  updateAmp();
  queue = xQueueCreate(8, sizeof(uint8_t));
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 1, nullptr, 0);
  sfx(SFX_HATCH);   // boot jingle: confirms the speaker works
}

void sfx(uint8_t id) {
  if (ready && enabled && queue) xQueueSend(queue, &id, 0);
}

// Not persisted here: the pet's save holds the sound setting (ui flags) and ui::begin()
// applies it at boot. A Preferences write would freeze both cores ~1 s on every boot.
void audioSetEnabled(bool on) {
  enabled = on;
  updateAmp();
}

bool audioEnabled() { return enabled; }

void audioSetSleeping(bool s) {
  if (asleep == s) return;
  asleep = s;
  updateAmp();
}

}  // namespace hw
