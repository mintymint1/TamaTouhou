#include "hw_touch.h"

#include <Arduino.h>
#include <Wire.h>

#include "TouchDrvCSTXXX.hpp"
#include "board.h"

namespace hw {

static TouchDrvCST92xx touch;
static volatile bool irq = false;
static bool down = false;
static void IRAM_ATTR onTouchIrq() { irq = true; }

bool touchBegin() {
  touch.setPins(TP_RESET, TP_INT);
  bool ok = false;
  for (int i = 0; i < 3 && !ok; i++) {        // sometimes fails on the first try
    ok = touch.begin(Wire, TOUCH_ADDR, IIC_SDA, IIC_SCL);
    if (!ok) delay(150);
  }
  if (!ok) {
    Serial.println("CST9217 not found");
    return false;
  }
  touch.reset();                              // begin() leaves it in command mode
  touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  touch.setMirrorXY(true, true);              // panel is mounted rotated 180 degrees
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TP_INT), onTouchIrq, FALLING);
  return true;
}

bool touchRead(bool &pressed, int16_t &x, int16_t &y) {
  static uint32_t lastPoll = 0;
  uint32_t now = millis();
  if (now - lastPoll < 20) return false;      // 50 Hz is plenty for a finger
  lastPoll = now;
  if (!irq && !down) return false;
  irq = false;
  // Address-only transaction first: wakes the chip so getPoint() doesn't hit the driver's
  // 1000 ms I2C timeout (SensorLib ignores Wire.setTimeOut). Measured: 5 stalls/min -> 0.
  Wire.beginTransmission(TOUCH_ADDR);
  if (Wire.endTransmission() != 0) return false;
  int16_t tx, ty;
  pressed = touch.getPoint(&tx, &ty, 1) > 0;
  if (pressed) { x = tx; y = ty; }
  down = pressed;
  return true;
}

}  // namespace hw
