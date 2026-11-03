#!/usr/bin/env python3
"""Validate a theme and generate the firmware's theme tables (docs/themes.md).

  .venv/bin/python tools/build_theme.py touhou            # validate + write firmware/lib/theme_gen/
  .venv/bin/python tools/build_theme.py touhou --check    # validate only (CI)
  .venv/bin/python tools/build_theme.py touhou --pack     # also pack art into firmware/data/ (LittleFS)

Writes firmware/lib/theme_gen/theme_data.h / theme_data.cpp: the roster (species, keys,
rarity tiers, evolution edges, habitats, accents, favourite foods), starters, tunables,
the UI palette and the theme's text (vocabulary + strings/en.yaml). The engine
(firmware/lib/engine) only sees tama::ThemeDef; the UI reads the text/palette helpers.

--pack writes firmware/data/{sprites,items,bg}/*.spr (format: firmware/lib/engine/sprite_pack.h):
one palette per species (+ the rare-variant palette sampled from sprites/<id>/variant/), each
slot cropped to its own bounding box, run-length encoded, then decoded again and compared
with the source pixels. `pio run -t uploadfs` flashes firmware/data/ to the board.

The generated files are committed; CI regenerates them and fails on a diff.
"""
import argparse
import os
import sys

import yaml
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'firmware', 'lib', 'theme_gen')
DEFAULT_TIERS = [  # engine defaults for themes without rarity_tiers (TamaPoke's numbers)
    {'id': 'legendary', 'odds': 3, 'blessed_odds': 10, 'unlock_collected': 25},
    {'id': 'rare', 'odds': 27, 'blessed_odds': 45, 'unlock_collected': 0},
    {'id': 'common', 'odds': 'rest'},
]
NAME_MAX = 14          # header shows "NAME Lv.123" at text size 2 on a 466 px round screen
MAX_SPECIES = 256


def hash16(s):
    """FNV-1a folded to 16 bits; must match tama::hash16 in firmware/lib/engine/theme_def.h."""
    h = 2166136261
    for b in s.encode():
        h ^= b
        h = (h * 16777619) & 0xFFFFFFFF
    return ((h >> 16) ^ (h & 0xFFFF)) & 0xFFFF


def rgb565(hexcol):
    h = hexcol.lstrip('#')
    r, g, b = int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


def cstr(s):
    return '"' + str(s).replace('\\', '\\\\').replace('"', '\\"') + '"'


class ThemeError(Exception):
    pass


def load(theme_id):
    tdir = os.path.join(ROOT, 'themes', theme_id)
    theme = yaml.safe_load(open(os.path.join(tdir, 'theme.yaml')))
    roster = yaml.safe_load(open(os.path.join(tdir, 'roster.yaml')))
    sp = os.path.join(tdir, 'strings', 'en.yaml')
    strings = (yaml.safe_load(open(sp)) or {}) if os.path.exists(sp) else {}
    return tdir, theme, roster, strings


def validate(theme, roster):
    errs = []
    ids = [s['id'] for s in roster]
    dup = sorted({i for i in ids if ids.count(i) > 1})
    if dup:
        errs.append(f'duplicate species ids: {dup}')
    if len(roster) > MAX_SPECIES:
        errs.append(f'{len(roster)} species > {MAX_SPECIES}')
    keys = {}
    for i in ids:
        k = hash16(i)
        if k in keys and keys[k] != i:
            errs.append(f'hash16 collision: {i} / {keys[k]} (rename one)')
        keys[k] = i
    by_id = {s['id']: s for s in roster}
    tiers = theme.get('rarity_tiers') or DEFAULT_TIERS
    tier_ids = [t['id'] for t in tiers]
    if tiers[-1].get('odds') != 'rest':
        errs.append("the last rarity tier must be the catch-all (odds: rest)")
    if any(t.get('odds') == 'rest' for t in tiers[:-1]):
        errs.append("only the last rarity tier may use odds: rest")
    habitats = [h['id'] for h in theme.get('habitats', [])]
    targets = set()
    for s in roster:
        for e in s.get('evolves') or []:
            if e['to'] not in by_id:
                errs.append(f"{s['id']}: evolves to unknown species {e['to']}")
            targets.add(e['to'])
        if s.get('habitat') not in habitats:
            errs.append(f"{s['id']}: unknown habitat {s.get('habitat')}")
        if len(s['name']) > NAME_MAX:
            errs.append(f"{s['id']}: name {s['name']!r} longer than {NAME_MAX}")
        r = s.get('rarity')
        if r != 'evo' and r not in tier_ids:
            errs.append(f"{s['id']}: rarity {r!r} is not a tier ({tier_ids}) or 'evo'")
    for s in roster:   # evo targets are 'evo'; base forms are a tier
        if s['id'] in targets and s.get('rarity') != 'evo':
            errs.append(f"{s['id']}: is an evolution target, so rarity must be 'evo'")
        if s['id'] not in targets and s.get('rarity') == 'evo':
            errs.append(f"{s['id']}: rarity 'evo' but nothing evolves into it (unreachable)")
    # no cycles
    def walk(sid, seen):
        if sid in seen:
            errs.append(f'evolution cycle through {sid}')
            return
        for e in by_id.get(sid, {}).get('evolves') or []:
            walk(e['to'], seen | {sid})
    for s in roster:
        if s['id'] not in targets:
            walk(s['id'], set())
    for tid in tier_ids[:-1]:
        if not any(s.get('rarity') == tid for s in roster):
            errs.append(f'tier {tid!r} has no base forms')
    starters = theme.get('starters') or []
    for st in starters:
        if st not in by_id:
            errs.append(f'starter {st} is not a species')
        elif st in targets:
            errs.append(f'starter {st} is an evolved form')
    for s in roster:
        ff = s.get('favourite_food')
        if ff is not None and ff not in (0, 1, 2):
            errs.append(f"{s['id']}: favourite_food must be 0, 1 or 2")
    if errs:
        raise ThemeError('\n'.join(errs))
    return tiers, habitats


def generate(theme_id, theme, roster, strings, tiers, habitats):
    idx = {s['id']: i for i, s in enumerate(roster)}
    kinds = list((theme.get('kinds') or {}).keys())
    tier_ix = {t['id']: i for i, t in enumerate(tiers)}
    pal = theme.get('palette') or {}
    voc = theme.get('vocabulary') or {}
    tun = theme.get('tunables') or {}
    cpp = [
        f'// GENERATED by tools/build_theme.py from themes/{theme_id}/ - do not edit',
        '#include "theme_data.h"',
        '',
        'namespace tama {',
        'namespace {',
        '',
    ]
    for s in roster:
        edges = s.get('evolves') or []
        if edges:
            body = ', '.join(f'{{{idx[e["to"]]}, {int(e["level"])}}}' for e in edges)
            cpp.append(f'const EvoEdge kEvo_{s["id"]}[] = {{{body}}};')
    cpp += ['', 'const SpeciesDef kSpecies[] = {']
    for s in roster:
        edges = s.get('evolves') or []
        tier = 'kTierEvo' if s['rarity'] == 'evo' else str(tier_ix[s['rarity']])
        ff = s.get('favourite_food', hash16(s['id']) % 3)
        kind = kinds.index(s['kind']) + 1 if s.get('kind') in kinds else 0
        acc = rgb565(s.get('accent', pal.get('accent_default', '#c8283a')))
        evo = f'kEvo_{s["id"]}' if edges else 'nullptr'
        cpp.append(f'  {{{cstr(s["id"])}, 0x{hash16(s["id"]):04X}, {cstr(s["name"])}, {tier}, '
                   f'{habitats.index(s["habitat"])}, 0x{acc:04X}, {ff}, {kind}, {len(edges)}, {evo}}},'
                   f'  // {idx[s["id"]]}')
    cpp.append('};')
    cpp += ['', 'const TierDef kTiers[] = {']
    for t in tiers:
        rest = t.get('odds') == 'rest'
        odds = 0.0 if rest else float(t['odds'])
        bl = 0.0 if rest else float(t.get('blessed_odds', t['odds']))
        cpp.append(f'  {{{cstr(t["id"])}, {odds:.4f}f, {bl:.4f}f, {int(t.get("unlock_collected", 0))}, '
                   f'{"true" if rest else "false"}}},')
    cpp.append('};')
    starters = [idx[st] for st in theme.get('starters') or []]
    cpp += ['', f'const uint16_t kStarters[] = {{{", ".join(map(str, starters)) or "0"}}};', '']
    cpp.append('const char *const kHabitats[] = {' + ', '.join(cstr(h) for h in habitats) + '};')
    cpp.append('const char *const kTierHints[] = {' + ', '.join(cstr(t.get('hint', '')) for t in tiers) + '};')
    text = {f'V_{k.upper()}': v for k, v in voc.items() if isinstance(v, str)}
    for k, v in voc.items():
        if isinstance(v, list):
            for i, item in enumerate(v):
                text[f'V_{k.upper()}_{i}'] = item
    text.update({k: v for k, v in strings.items() if isinstance(v, str)})
    cpp += ['', 'const ThemeText kText[] = {']
    for k in sorted(text):
        cpp.append(f'  {{{cstr(k)}, {cstr(text[k])}}},')
    cpp += ['};', '', '}  // namespace', '']
    cpp += [
        'const ThemeDef kTheme = {',
        f'  {cstr(theme["id"])}, {int(theme.get("version", 1))},',
        f'  kSpecies, {len(roster)},',
        f'  kTiers, {len(tiers)},',
        f'  kStarters, {len(starters)},',
        f'  {int(tun.get("variant_odds", 48))}, {int(tun.get("farewell_age_days", 3))},',
        '};',
        '',
        'const char *themeHabitat(uint8_t i) { return i < kHabitatCount ? kHabitats[i] : ""; }',
        'const char *themeTierHint(uint8_t tier) { return tier < sizeof(kTierHints) / sizeof(kTierHints[0]) ? kTierHints[tier] : ""; }',
        '',
        'const char *themeText(const char *key, const char *fallback) {',
        '  for (const ThemeText &t : kText)',
        '    if (strcmp(t.key, key) == 0) return t.text;',
        '  return fallback;',
        '}',
        '',
        '}  // namespace tama',
        '',
    ]
    palette_keys = ['bg_day', 'bg_night', 'ink', 'ink_night', 'bar_ok', 'bar_warn', 'bar_bad', 'track',
                    'accent_default', 'clean_fx']
    defaults = {'bg_day': '#f2efe1', 'bg_night': '#141828', 'ink': '#2a2a36', 'ink_night': '#d8dcf0',
                'bar_ok': '#58b868', 'bar_warn': '#e8a23c', 'bar_bad': '#e8503a', 'track': '#d8d2bd',
                'accent_default': '#c8283a', 'clean_fx': '#ffffff'}
    h = [
        f'// GENERATED by tools/build_theme.py from themes/{theme_id}/ - do not edit',
        '#pragma once',
        '#include <cstdint>',
        '#include <cstring>',
        '#include "theme_def.h"',
        '',
        'namespace tama {',
        '',
        'extern const ThemeDef kTheme;',
        f'constexpr uint8_t kHabitatCount = {len(habitats)};',
        f'constexpr uint16_t kSpeciesCount = {len(roster)};',
        '',
        '// UI palette (RGB565)',
    ]
    for k in palette_keys:
        h.append(f'constexpr uint16_t kPal_{k} = 0x{rgb565(pal.get(k, defaults[k])):04X};')
    h += [
        '',
        'struct ThemeText { const char *key; const char *text; };',
        'const char *themeText(const char *key, const char *fallback);  // vocabulary (V_*) and S_* strings',
        'const char *themeHabitat(uint8_t i);',
        'const char *themeTierHint(uint8_t tier);',
        '',
        '}  // namespace tama',
        '',
    ]
    return '\n'.join(h), '\n'.join(cpp)


# --------------------------------------------------------------------------- art packing

DATA = os.path.join(ROOT, 'firmware', 'data')
SLOT_IDS = {'idle': 0, 'walk': 1, 'sleep': 2, 'eat': 3, 'sad': 4, 'happy': 5, 'flair1': 6, 'flair2': 7, 'flair3': 8}
DEFAULT_MS = {'idle': 200, 'walk': 120, 'sleep': 500, 'eat': 150, 'sad': 250, 'happy': 120}
PINGPONG_DEFAULT = {'sleep', 'sad'}   # pose-holding loops; anim.yaml `mode: loop|pingpong` overrides
OPAQUE = 128


def slot_frames(path, meta, slot):
    im = Image.open(path).convert('RGBA')
    m = meta or {}
    f = (m.get(slot) or {}).get('frame') if isinstance(m.get(slot), dict) else None
    g = m.get('frame')
    fw = f[0] if f else (g[0] if g and g[1] == im.height else im.height)
    if im.width % fw:
        raise ThemeError(f'{path}: width {im.width} not a multiple of frame width {fw}')
    frames = [im.crop((i * fw, 0, (i + 1) * fw, im.height)) for i in range(im.width // fw)]
    sm = m.get(slot) if isinstance(m.get(slot), dict) else {}
    if sm.get('ms'):
        ms = int(sm['ms'] if isinstance(sm['ms'], int) else sm['ms'][0])
    elif sm.get('fps'):
        ms = round(1000 / float(sm['fps']))
    else:
        ms = DEFAULT_MS.get(slot, 150)
    pingpong = sm.get('mode', 'pingpong' if slot in PINGPONG_DEFAULT else 'loop') == 'pingpong'
    return frames, ms, pingpong


def crop_slot(frames):
    boxes = [fr.getchannel('A').point(lambda a: 255 if a >= OPAQUE else 0).getbbox() for fr in frames]
    boxes = [b for b in boxes if b]
    if not boxes:
        raise ThemeError('slot has no visible pixels')
    box = (min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes))
    w, h = box[2] - box[0], box[3] - box[1]
    if w > 255 or h > 255:
        raise ThemeError(f'slot content {w}x{h} exceeds 255 px')
    return [fr.crop(box) for fr in frames], box


def round565(c):
    """The colour the RGB565 panel will actually show, expanded back to 8 bits."""
    r, g, b = c
    r, g, b = r >> 3, g >> 2, b >> 3
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))


def build_palette(images):
    """One <= 255-colour palette over every opaque pixel of a species (all slots).

    Colours are first rounded to what the RGB565 panel can show, which merges many of the
    AI shading's near-duplicates for free. Only if that still leaves > 255 colours is the set
    reduced with a median cut weighted by pixel count (so big areas keep their exact colour
    and stray anti-aliasing shades get merged). Returns (palette, {source rgb: index})."""
    counts, rounded = {}, {}
    for im in images:
        for n, (r, g, b, a) in im.getcolors(1 << 24) or []:
            if a < OPAQUE:
                continue
            src = (r, g, b)
            q = rounded.setdefault(src, round565(src))
            counts[q] = counts.get(q, 0) + n
    uniq = sorted(counts, key=lambda c: -counts[c])
    if len(uniq) <= 255:
        idx = {c: i for i, c in enumerate(uniq)}
        return uniq, {src: idx[q] for src, q in rounded.items()}
    sample = []
    for c in uniq:   # pixel-count weighted sample, capped so huge flat areas don't swamp it
        sample += [c] * min(counts[c], 400)
    img = Image.new('RGB', (len(sample), 1))
    img.putdata(sample)
    q = img.quantize(colors=255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    raw = q.getpalette()[:255 * 3]
    pal = [tuple(raw[k:k + 3]) for k in range(0, len(raw), 3)]
    strip = Image.new('RGB', (len(uniq), 1))
    strip.putdata(uniq)
    qim = strip.quantize(palette=q, dither=Image.Dither.NONE)
    mapped = list(qim.get_flattened_data() if hasattr(qim, 'get_flattened_data') else qim.getdata())
    idx = {c: mapped[k] for k, c in enumerate(uniq)}
    return pal, {src: idx[qq] for src, qq in rounded.items()}


def rle_frame(im, cmap):
    out, opaque = bytearray(), 0
    w, h = im.size
    px = im.load()
    for y in range(h):
        x = 0
        while x < w:
            if px[x, y][3] < OPAQUE:
                n = 0
                while x < w and px[x, y][3] < OPAQUE and n < 128:
                    x += 1
                    n += 1
                out.append(0x80 | (n - 1))
            else:
                run = []
                while x < w and px[x, y][3] >= OPAQUE and len(run) < 128:
                    run.append(cmap[px[x, y][:3]])
                    x += 1
                out.append(len(run) - 1)
                out += bytes(run)
                opaque += len(run)
    return bytes(out), opaque


def decode_frame(data, off, w, h):
    """Python mirror of SpritePack::decode, for verifying what we wrote."""
    pix, p = {}, off
    for y in range(h):
        x = 0
        while x < w:
            c = data[p]
            p += 1
            n = (c & 0x7F) + 1
            if not c & 0x80:
                for i in range(n):
                    pix[(x + i, y)] = data[p + i]
                p += n
            x += n
    return pix


def pack_sprite(slots, out_path, variant_slots=None):
    """slots: {slot: (frames, ms, pingpong)} -> .spr; variant_slots: same geometry, recoloured."""
    cropped, flags = {}, {}
    for slot, (frames, ms, pp) in slots.items():
        cf, box = crop_slot(frames)
        cropped[slot] = (cf, ms, box)
        flags[slot] = 1 if pp else 0
    pal, cmap = build_palette([f for cf, _, _ in cropped.values() for f in cf])
    to565 = [((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3) for r, g, b in pal]
    var565 = None
    if variant_slots:   # sample the variant colour at a pixel where each palette index occurs
        votes = [dict() for _ in pal]
        for slot, (cf, ms, box) in cropped.items():
            if slot not in variant_slots:
                continue
            vframes = [vf.crop(box) for vf in variant_slots[slot][0]]
            for f, vf in zip(cf, vframes):
                a, b = f.load(), vf.load()
                for y in range(f.height):
                    for x in range(f.width):
                        if a[x, y][3] >= OPAQUE:
                            i = cmap[a[x, y][:3]]
                            c = b[x, y][:3]
                            votes[i][c] = votes[i].get(c, 0) + 1
        vp = [max(v, key=v.get) if v else pal[i] for i, v in enumerate(votes)]
        var565 = [((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3) for r, g, b in vp]
    head = bytearray(b'SPR1') + (1).to_bytes(2, 'little') + len(pal).to_bytes(2, 'little')
    for c in to565:
        head += c.to_bytes(2, 'little')
    head.append(1 if var565 else 0)
    for c in var565 or []:
        head += c.to_bytes(2, 'little')
    order = sorted(cropped, key=lambda s: SLOT_IDS[s])
    head.append(len(order))
    table_len = sum(8 + 6 * len(cropped[s][0]) for s in order)
    pos = len(head) + table_len
    table, blobs = bytearray(), bytearray()
    for slot in order:
        cf, ms, box = cropped[slot]
        if len(cf) > 24:
            raise ThemeError(f'{out_path}: {slot} has {len(cf)} frames (max 24)')
        w, h = cf[0].size
        table += bytes([SLOT_IDS[slot], len(cf), w, h, h, flags[slot]]) + int(ms).to_bytes(2, 'little')
        for f in cf:
            data, opaque = rle_frame(f, cmap)
            table += (pos + len(blobs)).to_bytes(4, 'little') + opaque.to_bytes(2, 'little')
            blobs += data
    blob = bytes(head + table + blobs)
    # verify: decode every frame and compare indices with the source pixels
    o = len(head)
    for slot in order:
        cf, ms, box = cropped[slot]
        w, h = cf[0].size
        o += 8
        for f in cf:
            off = int.from_bytes(blob[o:o + 4], 'little')
            o += 6
            got = decode_frame(blob, off, w, h)
            px = f.load()
            want = {(x, y): cmap[px[x, y][:3]] for y in range(h) for x in range(w) if px[x, y][3] >= OPAQUE}
            if got != want:
                raise ThemeError(f'{out_path}: {slot} frame failed to round-trip')
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    open(out_path, 'wb').write(blob)
    return len(blob), len(pal), bool(var565)


def load_species_slots(d):
    meta_p = os.path.join(d, 'anim.yaml')
    meta = yaml.safe_load(open(meta_p)) if os.path.exists(meta_p) else {}
    slots = {}
    for slot in SLOT_IDS:
        p = os.path.join(d, slot + '.png')
        if os.path.exists(p):
            frames, ms, pp = slot_frames(p, meta, slot)
            slots[slot] = (frames, ms, pp)
    return slots, meta


THUMB_MAX = 36   # thumbnails fit 36x36, drawn at 2x in the 76 px Chronicle / Storage cells


def thumb_frame(im):
    """Idle frame 0 cropped to its content and shrunk (nearest) to fit THUMB_MAX."""
    bb = im.getchannel('A').point(lambda a: 255 if a >= OPAQUE else 0).getbbox()
    im = im.crop(bb) if bb else im
    k = max(im.width, im.height) / THUMB_MAX
    if k > 1:
        im = im.resize((max(1, round(im.width / k)), max(1, round(im.height / k))), Image.NEAREST)
    return im


def pack_thumbs(tdir, roster, out_path):
    """thumbs.bin: "TTH1" | u16 count | count x {u32 offset, u32 size} | SPR1 blobs (roster order;
    size 0 = no art yet). Each blob is a one-frame sprite with the variant palette when available."""
    blobs = []
    tmp = out_path + '.part'
    for s in roster:
        d = os.path.join(tdir, 'sprites', s['id'])
        if not os.path.exists(os.path.join(d, 'idle.png')):
            blobs.append(b'')
            continue
        slots, meta = load_species_slots(d)
        base = thumb_frame(slots['idle'][0][0])
        variant = None
        vp = os.path.join(d, (s.get('variant') or {}).get('sprites', 'variant/'), 'idle.png')
        if os.path.exists(vp):
            vf, _, _ = slot_frames(vp, meta, 'idle')
            v = vf[0]
            bb = slots['idle'][0][0].getchannel('A').point(lambda a: 255 if a >= OPAQUE else 0).getbbox()
            v = v.crop(bb) if bb else v
            if v.size != base.size:
                v = v.resize(base.size, Image.NEAREST)
            variant = {'idle': ([v], 0, False)}
        pack_sprite({'idle': ([base], 1000, False)}, tmp, variant)
        blobs.append(open(tmp, 'rb').read())
    if os.path.exists(tmp):
        os.remove(tmp)
    head = bytearray(b'TTH1') + len(blobs).to_bytes(2, 'little')
    pos = len(head) + 8 * len(blobs)
    for b in blobs:
        head += (pos if b else 0).to_bytes(4, 'little') + len(b).to_bytes(4, 'little')
        pos += len(b)
    data = bytes(head) + b''.join(blobs)
    open(out_path, 'wb').write(data)
    return len(data), sum(1 for b in blobs if b)


def pack(theme_id, tdir, roster):
    total, lines = 0, []
    for s in roster:
        d = os.path.join(tdir, 'sprites', s['id'])
        if not os.path.exists(os.path.join(d, 'idle.png')):
            continue
        slots, meta = load_species_slots(d)
        vdir = os.path.join(d, (s.get('variant') or {}).get('sprites', 'variant/'))
        vslots = {}
        if os.path.isdir(vdir):
            for slot in slots:
                p = os.path.join(vdir, slot + '.png')
                if os.path.exists(p):
                    vf, _, _ = slot_frames(p, meta, slot)
                    if len(vf) == len(slots[slot][0]) and vf[0].size == slots[slot][0][0].size:
                        vslots[slot] = (vf, 0)
        n, pc, hv = pack_sprite(slots, os.path.join(DATA, 'sprites', s['id'] + '.spr'), vslots or None)
        total += n
        lines.append(f"  {s['id']:14s} {n / 1024:6.1f} KB  {pc:3d} colours  slots {','.join(slots)}"
                     + ('  +variant' if hv else ''))
    for sub, kind in (('items', 'item'), ('backgrounds', 'bg')):
        src = os.path.join(tdir, sub)
        if not os.path.isdir(src):
            continue
        for f in sorted(os.listdir(src)):
            if not f.endswith('.png'):
                continue
            im = Image.open(os.path.join(src, f)).convert('RGBA')
            n, pc, _ = pack_sprite({'idle': ([im], 1000, False)}, os.path.join(DATA, 'bg' if kind == 'bg' else 'items', f[:-4] + '.spr'))
            total += n
            lines.append(f"  {kind}:{f[:-4]:10s} {n / 1024:6.1f} KB  {pc:3d} colours")
    n, have = pack_thumbs(tdir, roster, os.path.join(DATA, 'thumbs.bin'))
    total += n
    lines.append(f'  thumbs.bin     {n / 1024:6.1f} KB  {have}/{len(roster)} species')
    print('\n'.join(lines))
    print(f'packed {total / 1024:.0f} KB into {os.path.relpath(DATA, ROOT)}/ (LittleFS partition: 12.9 MB)')


def check_strings(strings):
    """Compare the theme's S_* overrides with the keys the firmware UI actually asks for
    (T("S_...") calls in firmware/lib/app). Unknown keys are usually typos or renamed IDs."""
    import glob
    import re
    used = set()
    for f in glob.glob(os.path.join(ROOT, 'firmware', 'lib', 'app', '*.cpp')):
        used |= set(re.findall(r'T\("(S_[A-Z0-9_]+)"', open(f).read()))
        used |= set(re.findall(r'"(S_[A-Z0-9_]+)"', open(f).read()))   # keys built in tables
    used |= {f'S_MEDAL_{i}' for i in range(8)}   # snprintf("S_MEDAL_%d")
    have = {k for k in strings if k.startswith('S_')}
    return sorted(have - used), sorted(used - have)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('theme')
    ap.add_argument('--check', action='store_true', help='validate only')
    ap.add_argument('--pack', action='store_true', help='also pack art into firmware/data/')
    args = ap.parse_args()
    tdir, theme, roster, strings = load(args.theme)
    try:
        tiers, habitats = validate(theme, roster)
    except ThemeError as e:
        print(f'theme {args.theme} is invalid:\n{e}')
        return 1
    unused, missing = check_strings(strings)
    if unused:
        print(f'WARNING: strings/en.yaml keys the firmware never uses (typo or renamed?): {", ".join(unused)}')
    if missing:
        print(f'note: {len(missing)} UI strings use the built-in English fallback: {", ".join(missing)}')
    print(f'theme {args.theme}: {len(roster)} species, tiers {[t["id"] for t in tiers]}, '
          f'{len(habitats)} habitats, starters {theme.get("starters")}: valid')
    if args.check:
        return 0
    h, cpp = generate(args.theme, theme, roster, strings, tiers, habitats)
    os.makedirs(OUT, exist_ok=True)
    for name, body in (('theme_data.h', h), ('theme_data.cpp', cpp)):
        path = os.path.join(OUT, name)
        if not os.path.exists(path) or open(path).read() != body:   # unchanged: keep mtime, no rebuild
            open(path, 'w').write(body)
    print(f'wrote {os.path.relpath(OUT, ROOT)}/theme_data.h, theme_data.cpp')
    if args.pack:
        try:
            pack(args.theme, tdir, roster)
        except ThemeError as e:
            print(f'packing failed: {e}')
            return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
