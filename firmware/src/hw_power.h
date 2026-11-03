#pragma once
// AXP2101 PMU (panel power rail, battery, PWR button) and PCF85063 RTC.
// Ported from TamaPoke's rtcbat.cpp (MIT).
#include <cstdint>

namespace hw {

bool powerBegin();          // call FIRST: enables BLDO1 (OLED VDD) before the display starts
int batteryPercent();       // 0-100, -1 = no battery (cached ~2 s: per-frame I2C reads flickered)
bool batteryCharging();
bool usbPresent();
int batteryMillivolts();
bool pwrShortPressed();     // PWR button short press (screen on/off); long 4 s = hardware power-off

bool rtcBegin();
uint32_t rtcEpoch();        // unix seconds, 0 if the RTC has no valid time
void rtcSetEpoch(uint32_t e);

}  // namespace hw
