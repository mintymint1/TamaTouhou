# Development

How to build, run and change the code. To install the finished game on a board, see [flashing.md](flashing.md); for the rules, [how-to-play.md](how-to-play.md).

## Setup

| You need | For | Install |
|---|---|---|
| Python 3.9+ | theme tools, PlatformIO | python.org, or your package manager |
| A C++17 compiler | tests, simulator | Xcode Command Line Tools (macOS), `build-essential` (Linux), MSYS2 (Windows) |
| CMake 3.20+ | simulator | `brew install cmake`, `apt install cmake` |
| The board (optional) | trying it on hardware | see [flashing.md](flashing.md) |

```bash
git clone https://github.com/mintymint1/TamaTouhou.git && cd TamaTouhou
make setup     # .venv with Pillow, PyYAML, pyserial and PlatformIO
make run       # the game in a window
```

The first `make run` downloads SDL3 (pinned by hash, into `firmware/.deps/`) and compiles it, which takes 1–2 minutes once. On Linux, SDL needs the usual X11/Wayland and audio development packages (`libx11-dev libxext-dev libwayland-dev libxkbcommon-dev libasound2-dev`, or your distribution's equivalents).

## Make targets

All of them run from the repository root.

| Command | What it does |
|---|---|
| `make run` | Pack the art, build and start the **simulator** |
| `make test` | Build and run the host **unit tests** (no board, no window) |
| `make check` | What CI runs: validate the theme, check the generated tables are committed, run the tests |
| `make data` | Validate the theme, regenerate `firmware/lib/theme_gen/` and pack the art into `firmware/data/` |
| `make board` | Build the **ESP32-S3 firmware** |
| `make flash` | Build and write firmware + art to a board on USB |
| `make monitor` | Serial console (115200) |
| `make release` | Board build + file-system image → `web/firmware/` (what the web flasher installs) |
| `make web` | Serve `web/` on `http://localhost:8000` to try the flasher locally (Chrome/Edge) |
| `make clean` | Remove build outputs (keeps the SDL download, saves and PlatformIO's cache) |

`THEME=<id>` picks another theme pack, and `ARGS="..."` passes options to the simulator, e.g. `make run ARGS="--zoom 2 --speed 2"`.

## The simulator

The simulator runs the exact same game code as the board (engine, screens, art) in a 466 × 466 window, with debug keys for testing.

**Mouse:** click = tap, drag = swipe, hold 3 s on the pet = release. **Arrow keys** = swipes.

| Key | Does |
|---|---|
| `1` / `2` / `3` | Game speed: real time / 1 game minute per second / 10 per second |
| `H` | Skip one game hour |
| `L` | +1 level |
| `F` / `E` | Fill / empty all needs |
| `D` | Jump to the next day (streaks) |
| `N` / `P` | Next / previous species (browse all sprites) |
| `V` | Toggle the rare colour |
| `R` | Make the pet ready to run away |
| `G` | Print 20 simulated egg rolls |
| `B` | Fill storage with every species and colour |
| `K` | Evolve now (leaves a keepsake) |
| `X` / `Shift+I` | Export the save to `<save dir>/export.tamasave` / import it back |
| `S` | Print stats |
| `M` | Mute |
| `Shift+W` | Wipe the save (new game) |
| `Q` / `Esc` | Quit |

Options: `--zoom N`, `--speed 1|2|3`, `--save DIR`, `--data DIR`, `--import FILE`.

The save lives in `firmware/sim/save/` (gitignored). The pet keeps ageing while the simulator is closed, like a board that's switched off.

**Headless screenshots** for docs and checking UI changes: replay taps and keys, then save one frame and quit.

```bash
firmware/sim/build/tamatouhou-sim --save /tmp/shot --taps "233,146;233,228" --keys "lllf" \
  --shot out.bmp --shot-ms 1500
```

## Code layout

```
firmware/
  lib/engine/     game rules, hardware-free C++17: pet*.cpp (lifecycle/care, eggs+evolution,
                  storage, saves), save_text (TAMA-SAVE files), sprite_pack (.spr decoder),
                  gesture (taps/swipes/holds), version.h
  lib/app/        screens: ui.cpp (router, main screen, egg, play), ui_status, ui_storage,
                  ui_chronicle, ui_hub (settings + hints), nav (screen stack), assets (art loading).
                  Draws through gfx.h and calls the platform through app_platform.h (plat::*).
  lib/theme_gen/  GENERATED from themes/<id>/ by tools/build_theme.py: roster, tiers, text, palette.
                  Committed, so the tests build without Python. Never edit by hand.
  src/            ESP32 only: pins (board.h), AXP2101 power + RTC, CO5300 display, CST9217 touch,
                  ES8311 audio, NVS saves, platform_esp32.cpp (plat::* for the board), main.cpp
  sim/            PC simulator on SDL3: platform_pc (plat::*), pc_canvas (same API as Arduino_GFX),
                  game (the loop), save_file, main.cpp
  test/host/      unit tests: framework.h + one test_*.cpp per area
  data/           GENERATED packed art for LittleFS and the simulator (gitignored)
themes/<id>/      a theme pack: theme.yaml, roster.yaml, strings/, sprites/, items/, backgrounds/
tools/            build_theme, variants, sprite_qa, theme_status, board_save, make_release
web/              the web flasher page; make release puts the firmware in web/firmware/
```

### How the pieces fit

```
themes/touhou/ --tools/build_theme.py--> lib/theme_gen/ (C++ tables)   + firmware/data/*.spr (art)
                                              |                               |
lib/engine (rules) <-- lib/app (screens) <----+                               |
        |                    |                                                |
        |        plat::* ----+---- src/ (board: LittleFS, NVS, RTC) <---------+
        |                    +---- sim/ (PC: files, SDL3) <-------------------+
        +-- test/host (rules + router + rotation, no hardware)
```

- **The engine never touches hardware.** Time comes in as arguments, randomness from an injected `Rng`, saves through a `KvStore`. That's what lets the tests and the simulator run the same rules as the board.
- **The engine never names a creature, place or theme word.** All of that comes from the theme pack. If you're about to type a species name or a theme colour into C++, it belongs in `themes/<id>/`.
- **One module per screen.** Navigation rules live in `nav.*` and are tested on their own.
- **Draw only through `gfx->`.** Screen rotation turns the canvas and maps touches back with `glassToScreen()`; writing the framebuffer directly would break it.

## Tests

```bash
make test                                 # all tests
make -C firmware/test/host FILTER=evo     # only tests whose name contains "evo"
make -C firmware/test/host asan           # with AddressSanitizer + UBSan
```

The suite covers the pet rules, storage, progression (including two whole-game playthroughs that print the pacing), milestones, save transfer and migration, the theme tables, the sprite decoder, gestures, navigation and rotation. **Every rule change comes with a test.** After a balance change, rerun the playthroughs and check the printed pacing.

## Saves

The board stores two blobs in NVS (namespace `t_<hash of theme id>`): `pet` (the active pet and player progress) and `box` (storage, written only when it changes). The simulator writes the same blobs as files.

- **Changing the saved state?** Bump the save version in `lib/engine/pet_format.h` and migrate the old layout in `readState()` (`pet_save.cpp`), with a test in `test_save_transfer.cpp` or `test_storage.cpp`.
- **Species are saved by `hash16(species id)`.** Never reuse or rename an id in a published theme, or saves will point at the wrong creature.
- Export/import packages both blobs with a CRC (`Pet::exportSave` / `importSave`), as text: `TAMA-SAVE 1 <theme>` followed by base64 (`save_text.h`). Import checks everything before it changes anything.

## Working on the board

The firmware uses the Arduino-ESP32 core 3.x through the [pioarduino](https://github.com/pioarduino/platform-espressif32) PlatformIO platform. The platform and the libraries are pinned in `firmware/platformio.ini`; bump them on purpose.

**Serial console commands** (`make monitor`): `STATS`, `LVL n`, `SPEC id`, `HATCH`, `EGGS`, `TIME epoch`, `ABANDON`, `WIPE`, `HEALTH`, `SAVE`, `LOAD <text>`.

**Hardware notes.** Each of these cost a bug on this board. Keep them in mind when touching `src/`:

- Turn on the AXP2101's **BLDO1** rail before `gfx->begin()`, or the screen stays black after the battery has run flat.
- **Touch (CST9217):** coordinates are mirrored (the panel is rotated 180°); reset it after `begin()`; read it only on its interrupt or while a finger is down, and send an address-only I2C transaction first, or the bus can freeze for a second.
- **NVS writes freeze both cores for about a second.** Save on user actions; leave periodic saves to moments when the screen is dimmed, off or the pet sleeps.
- `Serial.setTxTimeoutMs(0)`, or the game blocks when no USB host is listening.
- **Audio (ES8311 + NS4150B amp):** keep the amp on while sound is enabled; the codec needs its clock derived from BCLK (`reg01 = 0xBF`) and `reg44 = 0x58`; MCLK is GPIO 42.
- Use wrap-safe timers (`(int32_t)(deadline - millis())`), physics on real elapsed time, and counters sized for months of play.
- **The RTC has no backup battery.** Never let game time go backwards; the firmware reseeds from the last saved time.
- **AMOLED burn-in:** dim after 90 s, near-off after 5 min, and don't render while the screen is off.
- Render the whole frame into the PSRAM canvas, then push it. Never clear to black first (DMA flicker).
- Arduino defines a `bit()` macro, so avoid that name in shared code.
- **Partition table:** PlatformIO always writes `boot_app0.bin`, by default at `0xe000`, which is inside NVS. `partitions_16mb.csv` gives it an `otadata` slot at the end of flash and `platformio.ini` points it there. Keep the two in step if you change the layout.

## Themes

A theme is a folder of YAML and PNG files; the engine reads only what `tools/build_theme.py` generates from it. See [themes.md](themes.md) for the format and how to make one.

## Releasing

1. Bump `TAMA_VERSION` in `firmware/lib/engine/version.h` and add the changes to `CHANGELOG.md`.
2. Commit, then tag and push: `git tag v0.2.0 && git push origin v0.2.0`.
3. The **Release** workflow builds everything, attaches the images, `flash.sh` and `SHA256SUMS` to a GitHub release, and publishes the web flasher with the new firmware to GitHub Pages.

One-time repository setup: **Settings → Pages → Source: GitHub Actions**. Run the workflow by hand (Actions → Release → Run workflow) to republish the flasher without a new release.
