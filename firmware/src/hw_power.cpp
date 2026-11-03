#include "hw_power.h"

#include <Arduino.h>
#include <Wire.h>
#include <time.h>

#include "board.h"  // defines XPOWERS_CHIP_AXP2101 before XPowersLib
#include <SensorPCF85063.hpp>
#include <XPowersLib.h>

namespace hw {

static XPowersPMU pmu;
static SensorPCF85063 rtc;
static bool pmuOk = false, rtcOk = false;

// XPowersLib's charge-current steps: 25 mA up to 200 mA, then 100 mA up to 1000 mA. Always
// rounds DOWN to the nearest step (never charge harder than board.h asks).
static uint8_t chargeCurrentOpt(int mA) {
  if (mA < 100) mA = 100;
  if (mA > 1000) mA = 1000;
  if (mA < 300) return (uint8_t)(XPOWERS_AXP2101_CHG_CUR_100MA + ((mA > 200 ? 200 : mA) - 100) / 25);
  return (uint8_t)(XPOWERS_AXP2101_CHG_CUR_300MA + (mA - 300) / 100);
}

bool powerBegin() {
  pmuOk = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!pmuOk) {
    Serial.println("AXP2101 not found");
    return false;
  }
  // The AMOLED's VDD hangs off BLDO1. After a full battery drain the PMU resets with this
  // rail OFF and the screen stays black while everything else works (TamaPoke).
  pmu.setBLDO1Voltage(3300);
  pmu.enableBLDO1();
  // Charging from USB-C is done by the AXP2101 on its own; pin its settings to the fitted
  // battery (board.h) rather than trusting chip defaults, and keep the fuel gauge fed.
  pmu.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
  pmu.setChargerConstantCurr(chargeCurrentOpt(BATTERY_CHARGE_MA));
  pmu.enableBattDetection();
  pmu.enableBattVoltageMeasure();
  Serial.printf("charger: 4.2 V, %d mA, battery %s\n", BATTERY_CHARGE_MA,
                pmu.isBatteryConnect() ? "connected" : "not connected");
  // PWR button: short press handled in firmware, 4 s long press = hardware power-off
  // (the RTC keeps running, so time still passes).
  pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
  pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
  pmu.clearIrqStatus();
  return true;
}

// Power state is cached ~2 s: reading it over I2C every frame added bus traffic and made
// the brightness (which depends on USB presence) flicker.
static uint32_t cacheT = 0;
static int cachedPct = -1;
static bool cachedChg = false, cachedUsb = true;

static void refresh() {
  uint32_t now = millis();
  if (cacheT && now - cacheT < 2000) return;
  cacheT = now ? now : 1;
  if (!pmuOk) { cachedPct = -1; cachedChg = false; cachedUsb = true; return; }
  cachedPct = pmu.isBatteryConnect() ? pmu.getBatteryPercent() : -1;
  cachedChg = pmu.isCharging();
  cachedUsb = pmu.isVbusIn();
}

int batteryPercent() { refresh(); return cachedPct; }
bool batteryCharging() { refresh(); return cachedChg; }
bool usbPresent() { refresh(); return cachedUsb; }
int batteryMillivolts() { return pmuOk ? (int)pmu.getBattVoltage() : 0; }

bool pwrShortPressed() {
  if (!pmuOk) return false;
  pmu.getIrqStatus();
  bool hit = pmu.isPekeyShortPressIrq();
  if (hit) pmu.clearIrqStatus();
  return hit;
}

bool rtcBegin() {
  rtcOk = rtc.begin(Wire, IIC_SDA, IIC_SCL);
  if (!rtcOk) Serial.println("PCF85063 not found");
  return rtcOk;
}

uint32_t rtcEpoch() {
  if (!rtcOk) return 0;
  RTC_DateTime t = rtc.getDateTime();
  if (t.getYear() < 2025 || t.getYear() > 2120) return 0;   // never set / lost power
  struct tm tmv = {};
  tmv.tm_year = t.getYear() - 1900;
  tmv.tm_mon = t.getMonth() - 1;
  tmv.tm_mday = t.getDay();
  tmv.tm_hour = t.getHour();
  tmv.tm_min = t.getMinute();
  tmv.tm_sec = t.getSecond();
  time_t e = mktime(&tmv);   // TZ is UTC on the device
  return e > 0 ? (uint32_t)e : 0;
}

void rtcSetEpoch(uint32_t e) {
  if (!rtcOk) return;
  time_t tt = e;
  struct tm tmv;
  gmtime_r(&tt, &tmv);
  rtc.setDateTime(RTC_DateTime(tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                               tmv.tm_hour, tmv.tm_min, tmv.tm_sec));
}

}  // namespace hw
