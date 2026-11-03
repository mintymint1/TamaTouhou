#include "hw_display.h"

#include "board.h"

namespace hw {

static Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
static Arduino_CO5300 *panel = new Arduino_CO5300(bus, LCD_RESET, 0 /*rotation*/, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
Arduino_Canvas *gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);

bool displayBegin() {
  // 80 MHz QSPI (default 40): the flush is the frame-time bottleneck. If the panel ever
  // shows garbage, drop back to 40 MHz.
  if (!gfx->begin(80000000)) {
    Serial.println("display begin failed");
    return false;
  }
  panel->setBrightness(180);
  return true;
}

void setBrightness(uint8_t level) {
  static int current = -1;
  if (level == current) return;
  current = level;
  panel->setBrightness(level);
}

}  // namespace hw
