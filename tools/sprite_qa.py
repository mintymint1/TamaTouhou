#!/usr/bin/env python3
"""Check a theme's sprite strips, items and backgrounds before building a pack (free, read-only).

  .venv/bin/python tools/sprite_qa.py touhou
  .venv/bin/python tools/sprite_qa.py touhou --only reimu,marisa --sheet
  .venv/bin/python tools/sprite_qa.py path/to/theme_dir

Prints one line per finding:  ERROR|WARN <id> <slot> <message>   and exits 1 if there are errors.
--sheet also writes <theme>/art/qa_sheet.png: one row per species, frame 0 of every slot at 2x on
black, red border = error, yellow = warning.

Checks (docs/themes.md):
  sprites/<id>/<slot>.png
    slicing      strip width is a whole number of frames (per-slot `frame` in anim.yaml, then the
                 global `frame` if its height matches the strip, then square frames)
    empty        no frame without opaque pixels
    background   frame corners are transparent; < 2% of visible pixels semi-transparent
    edges        content doesn't touch the frame edge (clipped by the generator)
    colours      per species: warn > 16 (no 4-bit packing), > 255 (the builder quantises)
    size         idle content height 24-80 px (engine scale sanity)
    feet         within a slot, the lowest opaque row doesn't jump > 6 px between frames (except
                 happy/flair, which may leave the ground); across slots, the feet sit within 6 px
                 of idle's line, allowing for a centred larger canvas
    pop-in       idle: no frame sticking out > 4 px (flicker on loop); sleep/sad play ping-pong,
                 so only neighbouring-frame jumps > 6 px count
    facing       walk faces right: frame 0 is compared with the raw east/west rotations when an
                 art/raw/ export with rotations is present (skipped otherwise)
    roster       every sprite folder is a roster id; every roster species has idle.png (warn)
  items/*.png        transparent and non-empty; egg_0 no larger than 96 px
  backgrounds/*.png  232x156 and fully opaque
"""
import argparse
import glob
import os
import sys

import yaml
from PIL import Image, ImageDraw


def pixels(im):
    return im.get_flattened_data() if hasattr(im, 'get_flattened_data') else im.getdata()

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SLOT_ORDER = ['idle', 'walk', 'sleep', 'eat', 'sad', 'happy', 'flair1', 'flair2', 'flair3']
AIRBORNE = {'happy', 'flair1', 'flair2', 'flair3'}
OPAQUE = 128          # the builder thresholds alpha at 50%
FEET_TOL = 6
BG_SIZE = (232, 156)


def theme_dir(arg):
    if os.sep in arg or arg.startswith('.'):
        return os.path.abspath(arg)
    return os.path.join(ROOT, 'themes', arg)


class Findings:
    def __init__(self):
        self.items = []

    def add(self, level, sid, slot, msg):
        self.items.append((level, sid, slot, msg))
        print(f'{level:5s} {sid} {slot} {msg}')

    def level_for(self, sid, slot):
        """Worst finding for one cell; slot '-' = species-level (colours, unknown files)."""
        lv = [f[0] for f in self.items if f[1] == sid and f[2] == slot]
        return 'ERROR' if 'ERROR' in lv else ('WARN' if 'WARN' in lv else None)

    @property
    def errors(self):
        return sum(1 for f in self.items if f[0] == 'ERROR')

    @property
    def warnings(self):
        return sum(1 for f in self.items if f[0] == 'WARN')


# --------------------------------------------------------------------------- helpers

def frame_w(meta, slot, im):
    m = meta or {}
    f = (m.get(slot) or {}).get('frame') if isinstance(m.get(slot), dict) else None
    if f:
        return f[0], 'slot'
    g = m.get('frame')
    if g and g[1] == im.height:
        return g[0], 'global'
    return im.height, 'square'


def frames_of(im, fw):
    return [im.crop((i * fw, 0, (i + 1) * fw, im.height)) for i in range(im.width // fw)]


def opaque_bbox(fr):
    return fr.getchannel('A').point(lambda a: 255 if a >= OPAQUE else 0).getbbox()


def lowest_row(fr):
    bb = opaque_bbox(fr)
    return bb[3] - 1 if bb else None


def union_bbox(frs):
    boxes = [b for b in (opaque_bbox(f) for f in frs) if b]
    if not boxes:
        return None
    return (min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes))


def colours(frs):
    s = set()
    for f in frs:
        for n, (r, g, b, a) in f.getcolors(1 << 20) or []:
            if a >= OPAQUE:
                s.add((r, g, b))
    return s


def raw_rotation(theme, sid, direction):
    hits = glob.glob(os.path.join(theme, 'art', 'raw', 'creatures', sid, '*', 'rotations', f'{direction}.png'))
    return Image.open(hits[0]).convert('RGBA') if hits else None


def similarity_diff(a, b):
    """Mean alpha-mask difference of two sprites after cropping to content and resizing to 32x32."""
    def norm(im):
        bb = opaque_bbox(im)
        im = im.crop(bb) if bb else im
        return im.getchannel('A').point(lambda v: 255 if v >= OPAQUE else 0).resize((32, 32), Image.NEAREST)
    x, y = norm(a), norm(b)
    return sum(abs(p - q) for p, q in zip(pixels(x), pixels(y))) / (32 * 32 * 255)


# --------------------------------------------------------------------------- checks

def check_strip(f, theme, sid, slot, path, meta, ctx):
    im = Image.open(path).convert('RGBA')
    fw, source = frame_w(meta, slot, im)
    g = (meta or {}).get('frame')
    if source == 'square' and g and g[1] != im.height:
        f.add('WARN', sid, slot, f'anim.yaml frame {g} does not match strip height {im.height}; '
              f'sliced as {im.height}x{im.height} squares (add a per-slot `frame:`)')
    if fw <= 0 or im.width % fw:
        f.add('ERROR', sid, slot, f'strip width {im.width} is not a multiple of frame width {fw} ({source})')
        return None
    frs = frames_of(im, fw)
    ctx['cells'][slot] = frs[0]
    for i, fr in enumerate(frs):
        if not opaque_bbox(fr):
            f.add('ERROR', sid, slot, f'frame {i} is empty')
    # background
    for i, fr in enumerate(frs):
        w, h = fr.size
        corners = [fr.getpixel(p)[3] for p in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1))]
        if any(corners):
            f.add('ERROR', sid, slot, f'frame {i} corners not transparent (alpha {corners}): background not removed?')
            break
    alphas = [a for fr in frs for a in pixels(fr.getchannel('A')) if a]
    semi = sum(1 for a in alphas if a < 255)
    if alphas and semi / len(alphas) >= 0.02:
        f.add('WARN', sid, slot, f'{100 * semi / len(alphas):.1f}% of visible pixels are semi-transparent '
              f'(builder thresholds at 50%; edges may look ragged)')
    # clipped content
    ub = union_bbox(frs)
    if ub and (ub[0] == 0 or ub[1] == 0 or ub[2] == fw or ub[3] == im.height):
        f.add('WARN', sid, slot, f'content touches the frame edge (bbox {ub} in {fw}x{im.height}): clipped?')
    # pop-in: one frame's outline sticks out from the others (an object or limb that appears in
    # a single frame flickers on every loop). Breathing moves edges ~1 px; flag > 4 px.
    if slot in ('idle', 'sleep', 'sad') and len(frs) >= 3:
        boxes = [opaque_bbox(fr) for fr in frs]
        sm = (meta or {}).get(slot) if isinstance((meta or {}).get(slot), dict) else {}
        pingpong = sm.get('mode', 'pingpong' if slot in ('sleep', 'sad') else 'loop') == 'pingpong'
        if all(boxes) and pingpong:
            # ping-pong playback never wraps from the last frame to the first, so only a jump
            # between neighbouring frames is visible
            jumps = [i for i in range(1, len(boxes))
                     if max(abs(boxes[i][k] - boxes[i - 1][k]) for k in range(4)) > 6]
            if jumps:
                f.add('WARN', sid, slot, f'jump between frames {[(i - 1, i) for i in jumps]} '
                      f'(> 6 px outline change, ping-pong playback)')
        elif all(boxes):
            med = [sorted(b[k] for b in boxes)[len(boxes) // 2] for k in range(4)]
            odd = [i for i, b in enumerate(boxes) if max(abs(b[k] - med[k]) for k in range(4)) > 4]
            if odd:
                f.add('WARN', sid, slot, f'frame(s) {odd} stick out from the others (bbox '
                      f'{[boxes[i] for i in odd]} vs typical {tuple(med)}): something pops in/out')
    # feet
    lows = [lowest_row(fr) for fr in frs]
    lows = [r for r in lows if r is not None]
    if lows and slot not in AIRBORNE and max(lows) - min(lows) > FEET_TOL:
        f.add('WARN', sid, slot, f'feet move {max(lows) - min(lows)} px between frames')
    ctx['colours'] |= colours(frs)
    ctx['feet'][slot] = (max(lows) if lows else None, im.height)
    if slot == 'idle' and ub:
        h = ub[3] - ub[1]
        if not 24 <= h <= 80:
            f.add('WARN', sid, slot, f'idle content is {h} px tall (expected 24-80 for the engine scale)')
    return frs


def check_facing(f, theme, sid, frs):
    east, west = raw_rotation(theme, sid, 'east'), raw_rotation(theme, sid, 'west')
    if not (east and west and frs):
        return
    de, dw = similarity_diff(frs[0], east), similarity_diff(frs[0], west)
    if dw + 0.02 < de:
        f.add('WARN', sid, 'walk', f'looks like it faces LEFT (diff east {de:.2f} vs west {dw:.2f}); '
              'walk must face right')


def check_species(f, theme, sid):
    d = os.path.join(theme, 'sprites', sid)
    meta_p = os.path.join(d, 'anim.yaml')
    meta = yaml.safe_load(open(meta_p)) if os.path.exists(meta_p) else {}
    ctx = {'cells': {}, 'colours': set(), 'feet': {}}
    slots = sorted((p[:-4] for p in os.listdir(d) if p.endswith('.png')),
                   key=lambda s: SLOT_ORDER.index(s) if s in SLOT_ORDER else 99)
    unknown = [s for s in slots if s not in SLOT_ORDER]
    if unknown:
        f.add('WARN', sid, '-', f'unknown slot files {unknown} (not played by the engine)')
    if 'idle' not in slots:
        f.add('ERROR', sid, 'idle', 'idle.png missing (required)')
    for slot in slots:
        frs = check_strip(f, theme, sid, slot, os.path.join(d, slot + '.png'), meta, ctx)
        if slot == 'walk' and frs:
            check_facing(f, theme, sid, frs)
    n = len(ctx['colours'])
    if n > 255:
        f.add('WARN', sid, '-', f'{n} colours: build_theme.py quantises to 255 (RGB565-rounded, weighted median cut)')
    elif n > 16:
        f.add('WARN', sid, '-', f'{n} colours (> 16: no 4-bit packing)')
    base = ctx['feet'].get('idle')
    if base and base[0] is not None:
        idle_margin = base[1] - 1 - base[0]
        for slot, (low, h) in ctx['feet'].items():
            if slot == 'idle' or low is None or slot in AIRBORNE:
                continue
            margin = (h - 1 - low) - (h - base[1]) / 2      # a larger canvas is assumed centred
            if abs(margin - idle_margin) > FEET_TOL:
                f.add('WARN', sid, slot, f'feet sit {margin - idle_margin:+.0f} px from idle\'s line '
                      '(builder re-anchors; check for a shadow/effect below the feet)')
    return ctx['cells']


def check_items(f, theme):
    d = os.path.join(theme, 'items')
    if not os.path.isdir(d):
        return
    for p in sorted(glob.glob(os.path.join(d, '*.png'))):
        name = os.path.basename(p)[:-4]
        im = Image.open(p).convert('RGBA')
        if not opaque_bbox(im):
            f.add('ERROR', 'items', name, 'empty image')
            continue
        w, h = im.size
        corners = [im.getpixel(c)[3] for c in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1))]
        if any(corners):
            f.add('ERROR', 'items', name, f'corners not transparent (alpha {corners})')
        if name == 'egg_0' and max(w, h) > 96:
            f.add('WARN', 'items', name, f'{w}x{h}: larger than 96 px')


def check_backgrounds(f, theme):
    for p in sorted(glob.glob(os.path.join(theme, 'backgrounds', '*.png'))):
        name = os.path.basename(p)[:-4]
        im = Image.open(p).convert('RGBA')
        if im.size != BG_SIZE:
            f.add('ERROR', 'backgrounds', name, f'{im.size[0]}x{im.size[1]}, expected {BG_SIZE[0]}x{BG_SIZE[1]}')
        lo = im.getchannel('A').getextrema()[0]
        if lo < 255:
            f.add('ERROR', 'backgrounds', name, f'not fully opaque (min alpha {lo})')


# --------------------------------------------------------------------------- sheet

def write_sheet(theme, rows, f):
    if not rows:
        return None
    scale, pad, label = 2, 6, 12
    cw = max((c.width for _, cells in rows for c in cells.values()), default=32) * scale
    ch = max((c.height for _, cells in rows for c in cells.values()), default=32) * scale
    ncol = max(len(cells) for _, cells in rows)
    W = 110 + ncol * (cw + pad) + pad
    H = len(rows) * (ch + label + pad) + pad
    sheet = Image.new('RGBA', (W, H), (0, 0, 0, 255))
    dr = ImageDraw.Draw(sheet)
    for r, (sid, cells) in enumerate(rows):
        y = pad + r * (ch + label + pad)
        lv = f.level_for(sid, '-')
        ink = (230, 60, 60, 255) if lv == 'ERROR' else (230, 190, 40, 255) if lv == 'WARN' else (220, 220, 230, 255)
        dr.text((pad, y + label + ch // 2 - 6), sid, fill=ink)
        for c, slot in enumerate(sorted(cells, key=lambda s: SLOT_ORDER.index(s) if s in SLOT_ORDER else 99)):
            x = 110 + c * (cw + pad)
            dr.text((x, y), slot, fill=(150, 150, 160, 255))
            big = cells[slot].resize((cells[slot].width * scale, cells[slot].height * scale), Image.NEAREST)
            sheet.alpha_composite(big, (x, y + label + ch - big.height))
            lv = f.level_for(sid, slot)
            if lv:
                col = (230, 40, 40, 255) if lv == 'ERROR' else (230, 190, 40, 255)
                dr.rectangle((x - 2, y + label - 2, x + cw + 1, y + label + ch + 1), outline=col, width=2)
    out = os.path.join(theme, 'art', 'qa_sheet.png')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    sheet.save(out)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('theme', help='theme id (themes/<id>) or a path to a theme folder')
    ap.add_argument('--only', help='comma-separated species ids')
    ap.add_argument('--sheet', action='store_true', help='write art/qa_sheet.png')
    args = ap.parse_args()
    theme = theme_dir(args.theme)
    only = set(args.only.split(',')) if args.only else None
    f = Findings()

    roster_p = os.path.join(theme, 'roster.yaml')
    roster_ids = [s['id'] for s in yaml.safe_load(open(roster_p))] if os.path.exists(roster_p) else []
    sdir = os.path.join(theme, 'sprites')
    folders = sorted(d for d in os.listdir(sdir) if os.path.isdir(os.path.join(sdir, d))) if os.path.isdir(sdir) else []
    if roster_ids:
        for d in folders:
            if d not in roster_ids and (not only or d in only):
                f.add('WARN', d, '-', 'sprite folder is not a roster id (typo or renamed species?)')
    rows = []
    for sid in (roster_ids or folders):
        if only and sid not in only:
            continue
        if sid not in folders:
            if not only:
                f.add('WARN', sid, 'idle', 'no sprites yet (engine uses the placeholder)')
            continue
        rows.append((sid, check_species(f, theme, sid)))
    if not only:
        check_items(f, theme)
        check_backgrounds(f, theme)
    out = write_sheet(theme, rows, f) if args.sheet else None
    print(f'\n{f.errors} errors, {f.warnings} warnings, {len(rows)} species checked'
          + (f'; sheet: {os.path.relpath(out, ROOT)}' if out else ''))
    return 1 if f.errors else 0


if __name__ == '__main__':
    sys.exit(main())
