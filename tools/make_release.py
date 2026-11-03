#!/usr/bin/env python3
"""Collect a board build into web/firmware/ for the web flasher (ESP Web Tools).

  make release                                  # builds everything, then runs this
  .venv/bin/python tools/make_release.py        # after `pio run` + `pio run -t buildfs`

Writes web/firmware/:
  bootloader.bin  partitions.bin  firmware.bin  littlefs.bin
  manifest.json   the ESP Web Tools manifest: each file with its flash offset
  SHA256SUMS      checksums, also attached to GitHub releases
  flash.sh        the same install with esptool, for people without Chrome/Edge

The four images are listed separately on purpose. A single merged image (esptool merge-bin)
pads the gaps with 0xFF, and one of those gaps is NVS, where the pet's save lives: every
update would wipe the pet. Separate parts leave NVS alone; the flasher's "erase device"
checkbox is there for a clean start.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import sys

import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FW = os.path.join(ROOT, 'firmware')
BOOTLOADER_OFFSET = 0x0       # ESP32-S3 (the ESP32 classic uses 0x1000)
PARTITIONS_OFFSET = 0x8000


def version():
    text = open(os.path.join(FW, 'lib', 'engine', 'version.h')).read()
    m = re.search(r'#define TAMA_VERSION "([^"]+)"', text)
    if not m:
        sys.exit('no TAMA_VERSION in firmware/lib/engine/version.h')
    return m.group(1)


def partitions(csv_path):
    """name -> (offset, size) from the partition table."""
    out = {}
    for line in open(csv_path):
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        cols = [c.strip() for c in line.split(',')]
        if len(cols) >= 5:
            out[cols[0]] = (int(cols[3], 0), int(cols[4], 0))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--env', default='amoled175', help='PlatformIO environment')
    ap.add_argument('--theme', default='touhou', help='theme id, for the manifest name')
    ap.add_argument('--out', default=os.path.join(ROOT, 'web', 'firmware'))
    args = ap.parse_args()

    build = os.path.join(FW, '.pio', 'build', args.env)
    parts = partitions(os.path.join(FW, 'partitions_16mb.csv'))
    app_offset, app_size = parts['app0']
    fs_offset, fs_size = parts['spiffs']

    images = [
        ('bootloader.bin', BOOTLOADER_OFFSET),
        ('partitions.bin', PARTITIONS_OFFSET),
        ('firmware.bin', app_offset),
        ('littlefs.bin', fs_offset),
    ]
    for name, _ in images:
        if not os.path.exists(os.path.join(build, name)):
            sys.exit(f'missing {build}/{name}: run `make release` (it builds the firmware and the file system)')
    if os.path.getsize(os.path.join(build, 'firmware.bin')) > app_size:
        sys.exit('firmware.bin is bigger than the app partition')
    if os.path.getsize(os.path.join(build, 'littlefs.bin')) != fs_size:
        sys.exit('littlefs.bin does not match the LittleFS partition size: rebuild with `make release`')

    shutil.rmtree(args.out, ignore_errors=True)
    os.makedirs(args.out)
    sums = []
    for name, _ in images:
        dst = os.path.join(args.out, name)
        shutil.copyfile(os.path.join(build, name), dst)
        sums.append(f'{hashlib.sha256(open(dst, "rb").read()).hexdigest()}  {name}')

    ver = version()
    theme = yaml.safe_load(open(os.path.join(ROOT, 'themes', args.theme, 'theme.yaml')))
    manifest = {
        'name': f"TamaTouhou ({theme.get('name', args.theme)} theme)",
        'version': ver,
        'new_install_prompt_erase': True,     # offer "erase device" (off = keep the pet)
        'new_install_improv_wait_time': 0,    # the firmware has no Wi-Fi setup to wait for
        'builds': [{
            'chipFamily': 'ESP32-S3',
            'parts': [{'path': name, 'offset': off} for name, off in images],
        }],
    }
    with open(os.path.join(args.out, 'manifest.json'), 'w') as f:
        json.dump(manifest, f, indent=2)
        f.write('\n')
    with open(os.path.join(args.out, 'SHA256SUMS'), 'w') as f:
        f.write('\n'.join(sums) + '\n')

    flash_args = ' '.join(f'0x{off:x} {name}' for name, off in images)
    with open(os.path.join(args.out, 'flash.sh'), 'w') as f:
        f.write('#!/bin/sh\n'
                '# Flash without a browser: pip install esptool, then run this next to the .bin files.\n'
                '# Your pet\'s save (NVS) is kept. Add --erase-all after write-flash for a clean start.\n'
                'set -e\n'
                'cd "$(dirname "$0")"\n'
                f'esptool --chip esp32s3 --baud 921600 write-flash -z {flash_args}\n')
    os.chmod(os.path.join(args.out, 'flash.sh'), 0o755)

    print(f'web/firmware/ ready: version {ver}, {len(images)} images')
    for name, off in images:
        print(f'  0x{off:06x}  {name}')


if __name__ == '__main__':
    main()
