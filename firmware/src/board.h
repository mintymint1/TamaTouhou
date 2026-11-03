#pragma once
// Waveshare ESP32-S3-Touch-AMOLED-1.75 pin map (decision D1).
// From TamaPoke's pin_config.h (MIT), which took it from Waveshare's official repo and
// corrected the I2S MCLK pin on real hardware.

#define XPOWERS_CHIP_AXP2101

// Round 466x466 AMOLED, CO5300 driver over QSPI
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 39
#define LCD_WIDTH 466
#define LCD_HEIGHT 466

// Shared I2C bus: CST9217 touch (0x5A), AXP2101 PMU, PCF85063 RTC, ES8311 codec (0x18)
#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#define TP_RESET 40
#define TOUCH_ADDR 0x5A

// ES8311 audio. MCLK is GPIO42 (verified on the board; some docs say 16). The codec runs
// from a BCLK-derived clock anyway.
#define I2S_MCK_IO 42
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8
#define PA_ENABLE 46   // NS4150B amplifier

// LiPo battery on the MX1.25 connector, charged by the AXP2101 from USB-C. Charge current
// is set for the fitted cell: about 0.5C, never above 1C (its mAh rating). Example cell:
// 803040, 3.7 V, 1000 mAh -> 500 mA (~2-2.5 h from empty). Full voltage: 4.2 V (standard
// LiPo; a 4.35 V "LiHV" cell would need XPOWERS_AXP2101_CHG_VOL_4V35 in hw_power.cpp).
#define BATTERY_CHARGE_MA 500

// microSD (SDMMC 1-bit). Optional: theme packs live on the flash filesystem.
#define SDMMC_CLK 2
#define SDMMC_CMD 1
#define SDMMC_DATA 3

// Screen geometry (CX, CY, kScreenW/H) lives in lib/app/gfx.h, shared with the PC builds.
