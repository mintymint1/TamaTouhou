# Third-party notices

This project builds on other people's work. Thank you.

## Included in this repository

### TamaPoke (MIT)

The game engine, the ESP32 drivers (`firmware/src/`), the sound effects (`firmware/lib/app/sfx_table.h`) and many of the hardware fixes are ported from [TamaPoke](https://github.com/socquique/TamaPoke) by Quique Tortosa, with the Pokémon theme and the battle system removed. Comments that say "TamaPoke 1.x" point at the TamaPoke changelog entry that found the bug.

```
MIT License

Copyright (c) 2026 Quique Tortosa

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

No TamaPoke art (Pokémon sprites, PMD SpriteCollab) or case files are included.

### Adafruit GFX font (BSD)

`firmware/sim/glcdfont.h` is the classic 5×7 font from [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library), Copyright (c) 2012 Adafruit Industries, BSD license, as shipped in Arduino_GFX. The simulator uses it so its text matches the board's.

## Downloaded at build time (linked into the binaries)

| Component | Used for | License |
|---|---|---|
| [Arduino-ESP32](https://github.com/espressif/arduino-esp32) via [pioarduino](https://github.com/pioarduino/platform-espressif32) | the board's runtime | LGPL-2.1 (core), Apache-2.0 (ESP-IDF) |
| [GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX) (moononournation) | display driver + canvas | see its repository; includes Adafruit GFX code (BSD) |
| [SensorLib](https://github.com/lewisxhe/SensorLib) (Lewis He) | touch and RTC | MIT |
| [XPowersLib](https://github.com/lewisxhe/XPowersLib) (Lewis He) | AXP2101 power chip | MIT |
| [SDL3](https://github.com/libsdl-org/SDL) | the PC simulator | zlib |

The firmware links the LGPL-licensed Arduino-ESP32 core. The complete source of this project and the build instructions ([docs/development.md](docs/development.md)) are here, so you can rebuild the firmware against a modified core.

## Loaded by the web flasher

| Component | Used for | License |
|---|---|---|
| [ESP Web Tools](https://github.com/esphome/esp-web-tools) (ESPHome), from unpkg.com | the Install button and flashing over Web Serial | Apache-2.0 |

## Characters and names

Touhou Project characters, names and places belong to **ZUN / Team Shanghai Alice**. This is an unofficial, non-commercial fan work made under ZUN's fan-work guidelines, and it contains no material from the official games. "Tamagotchi" is a trademark of Bandai; this project isn't affiliated with or endorsed by Bandai, and neither is it affiliated with Waveshare.
