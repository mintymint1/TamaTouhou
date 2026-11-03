#!/usr/bin/env python3
"""Make every species' rare variant (TamaPoke's "shiny") from its sprites (docs/themes.md).

  .venv/bin/python tools/variants.py touhou                    # all species with a `variant:`
  .venv/bin/python tools/variants.py touhou --only reimu,marisa
  .venv/bin/python tools/variants.py touhou --palette reimu    # print reimu's colours (to write a palette_map)
  .venv/bin/python tools/variants.py path/to/theme_dir         # any theme folder (e.g. a scratch copy)

Reads  <theme>/roster.yaml and <theme>/sprites/<id>/*.png (one horizontal strip per slot)
Writes <theme>/sprites/<id>/variant/<slot>.png   for every slot strip present
       <theme>/art/variants_preview.png          idle frame 0, normal | variant, 3x, black background

`variant:` in roster.yaml (one of; palette_map + hue_shift may be combined):
  hue_shift: <deg>                 rotate the hue of non-grey pixels by <deg>
    hue_range: [lo, hi]            optional: only shift hues in this range (degrees, wraps past 360)
    protect_skin: true             optional: leave skin tones alone (characters with faces)
  palette_map: {"#rrggbb": "#rrggbb", ...}
                                   exact recolour; a pixel within `tolerance` (RGB distance, default 24)
                                   of a key is recoloured, keeping its small offset from the key so the
                                   shading of AI-art near-duplicates survives. Applied before hue_shift;
                                   pixels it recoloured are not hue-shifted again.
    tolerance: 24
  sprites: variant/                hand-made variant art: used as-is, never overwritten

Free and idempotent: no API calls, and re-running rewrites the same files. Generated variant
folders carry a `.generated` marker; a variant/ folder without it is treated as hand-made
and left alone.
"""
import argparse
import colorsys
import os
import sys

import yaml
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MARKER = '.generated'
SCALE = 3
BG = (0, 0, 0, 255)
INK = (210, 210, 220, 255)


def theme_dir(arg):
    if os.sep in arg or arg.startswith('.'):
        return os.path.abspath(arg)
    return os.path.join(ROOT, 'themes', arg)


def hexrgb(h):
    h = h.strip().lstrip('#')
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def in_hue_range(deg, rng):
    lo, hi = rng
    lo, hi = lo % 360, hi % 360
    return lo <= deg <= hi if lo <= hi else (deg >= lo or deg <= hi)


def is_skin(h, s, v):
    deg = h * 360
    return 5 <= deg <= 45 and 0.12 <= s <= 0.65 and v >= 0.45


# --------------------------------------------------------------------------- colour transform

def make_mapper(cfg):
    """Return f((r,g,b)) -> (r,g,b) for one species' variant config."""
    pmap = [(hexrgb(k), hexrgb(v)) for k, v in (cfg.get('palette_map') or {}).items()]
    tol2 = cfg.get('tolerance', 24) ** 2
    shift = cfg.get('hue_shift')
    rng = cfg.get('hue_range')
    protect = cfg.get('protect_skin', False)

    def f(rgb):
        best, bd = None, tol2 + 1
        for src, dst in pmap:
            d = sum((a - b) ** 2 for a, b in zip(rgb, src))
            if d < bd:
                best, bd = (src, dst), d
        if best:
            src, dst = best
            return tuple(max(0, min(255, c + (x - s))) for c, x, s in zip(dst, rgb, src))
        if shift is None:
            return rgb
        h, s, v = colorsys.rgb_to_hsv(*(c / 255 for c in rgb))
        if s < 0.12 or v < 0.08:                      # greys, black outline, white highlights
            return rgb
        if rng and not in_hue_range(h * 360, rng):
            return rgb
        if protect and is_skin(h, s, v):
            return rgb
        nr, ng, nb = colorsys.hsv_to_rgb((h + shift / 360) % 1.0, s, v)
        return (round(nr * 255), round(ng * 255), round(nb * 255))

    return f


def recolour(im, f):
    im = im.convert('RGBA')
    cache = {}
    out = []
    data = im.get_flattened_data() if hasattr(im, 'get_flattened_data') else im.getdata()
    for r, g, b, a in data:
        if a == 0:
            out.append((r, g, b, a))
            continue
        key = (r, g, b)
        if key not in cache:
            cache[key] = f(key)
        out.append(cache[key] + (a,))
    res = Image.new('RGBA', im.size)
    res.putdata(out)
    return res


# --------------------------------------------------------------------------- per species

def slot_files(d):
    return sorted(f for f in os.listdir(d) if f.endswith('.png') and os.path.isfile(os.path.join(d, f)))


def frame_w(meta, slot, im):
    """Frame width for one slot strip: per-slot `frame` in anim.yaml, else the global `frame`
    if its height matches this strip, else square frames (width = strip height). Custom v3
    animations come back on a larger canvas than idle/walk, so one global size isn't enough."""
    m = meta or {}
    f = (m.get(slot) or {}).get('frame') if isinstance(m.get(slot), dict) else None
    if f:
        return f[0]
    g = m.get('frame')
    if g and g[1] == im.height:
        return g[0]
    return im.height


def frame_width(d, im, slot='idle'):
    meta_p = os.path.join(d, 'anim.yaml')
    return frame_w(yaml.safe_load(open(meta_p)) if os.path.exists(meta_p) else {}, slot, im)


def do_species(tdir, sp):
    sid, cfg = sp['id'], sp.get('variant')
    d = os.path.join(tdir, 'sprites', sid)
    if not cfg:
        return None, f'{sid}: no `variant:` in roster, skipped'
    if not os.path.isdir(d) or not slot_files(d):
        return None, f'{sid}: no sprites yet, skipped'
    if 'sprites' in cfg:
        vdir = os.path.join(d, cfg['sprites'])
        if not os.path.isdir(vdir):
            return None, f'{sid}: hand-made variant folder {cfg["sprites"]} missing'
        return vdir, f'{sid}: hand-made variant ({len(slot_files(vdir))} strips, untouched)'
    vdir = os.path.join(d, 'variant')
    if os.path.isdir(vdir) and not os.path.exists(os.path.join(vdir, MARKER)) and slot_files(vdir):
        return vdir, f'{sid}: variant/ has no {MARKER} marker, so it looks hand-made; left alone'
    os.makedirs(vdir, exist_ok=True)
    open(os.path.join(vdir, MARKER), 'w').write('made by tools/variants.py; safe to delete and regenerate\n')
    f = make_mapper(cfg)
    slots = slot_files(d)
    for name in slots:
        recolour(Image.open(os.path.join(d, name)), f).save(os.path.join(vdir, name))
    for stale in set(slot_files(vdir)) - set(slots):      # slot removed since the last run
        os.remove(os.path.join(vdir, stale))
    how = ', '.join(k for k in ('palette_map', 'hue_shift') if cfg.get(k) is not None)
    return vdir, f'{sid}: {len(slots)} strips ({how})'


def idle_frame0(strip_dir, meta_dir):
    """Frame 0 of idle.png in strip_dir; frame size comes from meta_dir/anim.yaml."""
    p = os.path.join(strip_dir, 'idle.png')
    if not os.path.exists(p):
        return None
    im = Image.open(p).convert('RGBA')
    return im.crop((0, 0, min(frame_width(meta_dir, im), im.width), im.height))


def preview(tdir, pairs):
    if not pairs:
        return None
    cells = []
    for sid, d, vdir in pairs:
        a, b = idle_frame0(d, d), idle_frame0(vdir, d)
        if a is None or b is None:
            continue
        cells.append((sid, a, b))
    if not cells:
        return None
    cw = max(max(a.width, b.width) for _, a, b in cells) * SCALE
    ch = max(max(a.height, b.height) for _, a, b in cells) * SCALE
    pad, label = 8, 14
    per_row = 4
    rows = (len(cells) + per_row - 1) // per_row
    W = per_row * (2 * cw + 3 * pad)
    H = rows * (ch + label + 2 * pad)
    sheet = Image.new('RGBA', (W, H), BG)
    dr = ImageDraw.Draw(sheet)
    for i, (sid, a, b) in enumerate(cells):
        x0 = (i % per_row) * (2 * cw + 3 * pad) + pad
        y0 = (i // per_row) * (ch + label + 2 * pad) + pad
        dr.text((x0, y0), sid, fill=INK)
        for j, im in enumerate((a, b)):
            big = im.resize((im.width * SCALE, im.height * SCALE), Image.NEAREST)
            sheet.alpha_composite(big, (x0 + j * (cw + pad), y0 + label + ch - big.height))
    os.makedirs(os.path.join(tdir, 'art'), exist_ok=True)
    out = os.path.join(tdir, 'art', 'variants_preview.png')
    sheet.save(out)
    return out


def print_palette(tdir, sid, top=24):
    d = os.path.join(tdir, 'sprites', sid)
    counts = {}
    for name in slot_files(d):
        for n, (r, g, b, a) in Image.open(os.path.join(d, name)).convert('RGBA').getcolors(1 << 20) or []:
            if a:
                counts[(r, g, b)] = counts.get((r, g, b), 0) + n
    print(f'{sid}: {len(counts)} colours (top {top} by pixel count)')
    for (r, g, b), n in sorted(counts.items(), key=lambda kv: -kv[1])[:top]:
        h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
        print(f'  "#{r:02x}{g:02x}{b:02x}"  {n:6d} px   hue {h * 360:5.0f}  sat {s:.2f}  val {v:.2f}')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('theme', help='theme id (themes/<id>) or a path to a theme folder')
    ap.add_argument('--only', help='comma-separated species ids')
    ap.add_argument('--palette', metavar='ID', help='print a species\' colours instead of generating')
    args = ap.parse_args()
    tdir = theme_dir(args.theme)
    if args.palette:
        print_palette(tdir, args.palette)
        return
    roster = yaml.safe_load(open(os.path.join(tdir, 'roster.yaml')))
    only = set(args.only.split(',')) if args.only else None
    pairs, waiting = [], []
    for sp in roster:
        if only and sp['id'] not in only:
            continue
        vdir, msg = do_species(tdir, sp)
        if msg.endswith('no sprites yet, skipped'):
            waiting.append(sp['id'])
        else:
            print('  ' + msg)
        if vdir:
            pairs.append((sp['id'], os.path.join(tdir, 'sprites', sp['id']), vdir))
    if waiting:
        print(f'  waiting for sprites ({len(waiting)}): {", ".join(waiting)}')
    out = preview(tdir, pairs)
    print(f'preview: {os.path.relpath(out, ROOT) if out else "nothing to show"}')


if __name__ == '__main__':
    sys.exit(main())
