#!/usr/bin/env python3
"""Copy the board's savegame to/from a .tamasave file over USB (serial console SAVE / LOAD).

The file format is the same TAMA-SAVE text the simulator exports and imports (X / Shift+I,
--import FILE), so a pet can move between the board and a computer (same theme only).

  .venv/bin/python tools/board_save.py export my-pet.tamasave [--port /dev/cu.usbmodem101]
  .venv/bin/python tools/board_save.py import my-pet.tamasave [--port ...]

Close `make monitor` first: only one program can use the port at a time.
"""
import argparse
import glob
import sys
import time

import serial

TAG = 'TAMA-SAVE'


def find_port():
    ports = sorted(glob.glob('/dev/cu.usbmodem*') + glob.glob('/dev/ttyACM*') + glob.glob('/dev/ttyUSB*'))
    if not ports:
        sys.exit('no board found; pass --port')
    return ports[0]


def read_until(ser, want, timeout):
    """Lines from the board until one starts with any of `want`; returns that line."""
    end = time.time() + timeout
    while time.time() < end:
        line = ser.readline().decode(errors='replace').strip()
        if any(line.startswith(w) for w in want):
            return line
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('cmd', choices=['export', 'import'])
    ap.add_argument('file')
    ap.add_argument('--port')
    args = ap.parse_args()
    ser = serial.Serial(args.port or find_port(), 115200, timeout=1)
    time.sleep(0.5)
    ser.reset_input_buffer()

    if args.cmd == 'export':
        ser.write(b'SAVE\n')
        line = read_until(ser, [TAG, 'ERR'], 15)
        if not line or not line.startswith(TAG):
            sys.exit(f'export failed: {line or "no answer from the board"}')
        head, _, body = line.partition(' ')
        version, _, rest = body.partition(' ')
        theme, _, b64 = rest.partition(' ')
        wrapped = '\n'.join(b64[i:i + 76] for i in range(0, len(b64), 76))
        with open(args.file, 'w') as f:
            f.write(f'{head} {version} {theme}\n{wrapped}\n')
        print(f'saved {args.file} (theme {theme})')
        return

    text = ' '.join(open(args.file).read().split())   # the board takes it as one line
    if not text.startswith(TAG):
        sys.exit(f'{args.file} is not a TamaTouhou save')
    data = f'LOAD {text}\n'.encode()
    for i in range(0, len(data), 512):   # gentle chunks: the board reads while it plays
        ser.write(data[i:i + 512])
        time.sleep(0.01)
    line = read_until(ser, ['DONE', 'ERR'], 15)
    if line != 'DONE':
        sys.exit(f'import failed: {line or "no answer from the board"}')
    print('imported: the board is now running that save')


if __name__ == '__main__':
    main()
