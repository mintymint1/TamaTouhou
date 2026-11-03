# Installing the game on a board

**Hardware:** a [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm) (round 466 × 466 AMOLED, 16 MB flash, 8 MB PSRAM). A 3.7 V LiPo on the battery connector is optional; it charges over USB-C.

Pick one of three ways:

| Way | For | Needs |
|---|---|---|
| [1. Web flasher](#1-web-flasher-easiest) | Everyone | Chrome or Edge |
| [2. esptool](#2-esptool-no-chrome) | Firefox/Safari users, scripts | Python |
| [3. From source](#3-from-source) | Developers | This repository, Python, PlatformIO (installed for you) |

## 1. Web flasher (easiest)

1. Open **https://mintymint1.github.io/TamaTouhou/** in **Chrome** or **Edge** on a computer. Chrome on Android also works, with a USB-C OTG cable.
2. Connect the board with a USB-C **data** cable.
3. Click **Install on my board** and choose the board's port.
4. **First install:** tick **Erase device**. **Update:** leave it unticked to keep your pet.
5. Wait 2–4 minutes. The game is small, but the art image (~13 MB) takes most of the time. The board restarts on its own.

Firefox and Safari can't do this: they don't support Web Serial. iPhones and iPads can't either.

## 2. esptool (no Chrome)

1. Download `tamatouhou-vX.Y.Z.zip` from the [latest release](https://github.com/mintymint1/TamaTouhou/releases/latest) and unzip it.
2. Install esptool: `pip install esptool`.
3. Run `sh flash.sh` in the unzipped folder, or the same command by hand:

```bash
esptool --chip esp32s3 --baud 921600 write-flash -z \
  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin 0x310000 littlefs.bin
```

esptool finds the port on its own; add `--port /dev/ttyACM0` (Linux), `--port /dev/cu.usbmodem101` (macOS) or `--port COM5` (Windows) if it picks the wrong one. For a clean start, run `esptool --chip esp32s3 erase-flash` first. `SHA256SUMS` lets you check the downloads.

## 3. From source

```bash
make setup      # once: Python venv with PlatformIO and the theme tools
make flash      # build the game + art and write both to the board
make monitor    # optional: the serial console
```

`make flash` writes the program and the art. It never erases the save. See [development.md](development.md) for the rest.

## Troubleshooting

| Problem | Fix |
|---|---|
| No port to choose | Hold **BOOT**, plug the board in, then let go (download mode). Try another cable: many USB-C cables only charge. |
| Linux: "permission denied" | `sudo usermod -aG dialout $USER`, then log out and back in |
| Flash stops halfway | Run it again. A failed flash can't damage the board; the ESP32-S3's ROM bootloader is always there. |
| Flashing is slow or fails at 921600 baud | Use `--baud 460800` with esptool; the web flasher picks its own speed |
| Black screen after flashing | Unplug and replug. With a battery fitted, hold **PWR** for 4 s to switch it off, then press it again. |
| It boots but shows placeholder shapes instead of art | The art image wasn't written. Flash again; with `make`, run `make flash` (it writes both). |
| Pet gone after an update | "Erase device" was ticked. Restore a backup (below). |

The serial console (`make monitor`, or any serial terminal at 115200 baud) prints the firmware version and boot messages. Include them when you [report a bug](../CONTRIBUTING.md#reporting-bugs).

## Backing up your pet

The save holds your pet, THE GAP and your collection. It survives normal updates, but erasing the flash removes it. To keep a copy:

```bash
.venv/bin/python tools/board_save.py export my-pet.tamasave      # board -> file
.venv/bin/python tools/board_save.py import my-pet.tamasave      # file -> board
```

Run them from a clone of this repository after `make setup`. Add `--port ...` if more than one serial device is connected. The same `.tamasave` file also loads in the simulator (`--import FILE`), as long as the theme matches. The time between export and import passes for the pet like time switched off.

Without the repository, open the serial console and type `SAVE`. The board prints one line starting with `TAMA-SAVE`: copy and keep it. To restore, type `LOAD ` followed by that line.

## What's on the flash

| Offset | Size | Contents | Written by an update? |
|---|---|---|---|
| `0x0` | ~20 KB | Bootloader | yes |
| `0x8000` | 3 KB | Partition table | yes |
| `0x9000` | 24 KB | **NVS: your save** and device settings (screen rotation) | **no** (only by "erase") |
| `0x10000` | 3 MB | The game | yes |
| `0x310000` | ~12.9 MB | LittleFS: the theme's sprites and backgrounds | yes |
| `0xFEE000` | 8 KB | otadata (unused: there's no over-the-air update) | `make flash` only |
| `0xFF0000` | 64 KB | Core dump (crash reports) | no |

That's why the web flasher and `flash.sh` write four separate images instead of one merged file: a merged image would also overwrite NVS and wipe the pet.
