#!/usr/bin/env python3
"""Report which art a theme pack has and what's still missing.

  .venv/bin/python tools/theme_status.py touhou            # print the report
  .venv/bin/python tools/theme_status.py touhou --write    # also update themes/<id>/README.md
  .venv/bin/python tools/theme_status.py path/to/theme_dir # any theme folder (e.g. a scratch copy)

Species are grouped by rarity tier, rarest first. Tiers come from theme.yaml `rarity_tiers:`
(a list of {id, odds, blessed_odds, unlock_collected, hint}). Evolved forms (`rarity: evo`)
are listed under the tier of their line's base form. Themes without `rarity_tiers` use the
engine's default common/rare/legendary tiers.

The README section between the STATUS markers is regenerated; the rest of the README is
left alone. Slots and items follow docs/themes.md
"""
import os
import sys

import yaml
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SLOTS = ['idle', 'walk', 'sleep', 'eat', 'sad', 'happy']
ITEMS = ['egg_0', 'egg_1', 'egg_2', 'mess', 'emote', 'icon_feed', 'icon_play', 'icon_sleep',
         'icon_clean', 'food_0', 'food_1', 'food_2', 'treat', 'play_object']
DEFAULT_TIERS = [{'id': 'legendary', 'odds': 3}, {'id': 'rare', 'odds': 27}, {'id': 'common', 'odds': 'rest'}]
BEGIN, END = '<!-- STATUS:BEGIN (tools/theme_status.py) -->', '<!-- STATUS:END -->'


def theme_dir(arg):
    if os.sep in arg or arg.startswith('.'):
        return os.path.abspath(arg)
    return os.path.join(ROOT, 'themes', arg)


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


def frames(path):
    """Frame count of a horizontal strip."""
    im = Image.open(path)
    meta_p = os.path.join(os.path.dirname(path), 'anim.yaml')
    meta = yaml.safe_load(open(meta_p)) if os.path.exists(meta_p) else {}
    fw = frame_w(meta, os.path.basename(path)[:-4], im)
    return max(1, im.width // fw) if fw else 1


def tiers_rarest_first(theme):
    meta = os.path.join(theme, 'theme.yaml')
    tiers = (yaml.safe_load(open(meta)) or {}).get('rarity_tiers') if os.path.exists(meta) else None
    tiers = tiers or DEFAULT_TIERS

    def key(t):  # numeric odds ascending; 'rest' / missing (the catch-all tier) last
        o = t.get('odds')
        return (0, float(o)) if isinstance(o, (int, float)) else (1, 0.0)
    return sorted(tiers, key=key)


def line_tiers(roster):
    """species id -> tier id of its line's base form (evolved forms inherit it)."""
    parent = {}
    for s in roster:
        for e in s.get('evolves') or []:
            parent.setdefault(e['to'], s['id'])
    by_id = {s['id']: s for s in roster}
    out = {}
    for s in roster:
        cur, seen = s['id'], set()
        while by_id.get(cur, {}).get('rarity') == 'evo' and cur in parent and cur not in seen:
            seen.add(cur)
            cur = parent[cur]
        out[s['id']] = by_id.get(cur, s).get('rarity', 'evo')
    return out


def variant_idle(theme, s):
    sub = (s.get('variant') or {}).get('sprites', 'variant/')
    return os.path.join(theme, 'sprites', s['id'], sub, 'idle.png')


def species_section(theme):
    roster = yaml.safe_load(open(os.path.join(theme, 'roster.yaml')))
    tier_of = line_tiers(roster)
    tiers = tiers_rarest_first(theme)
    known = [t['id'] for t in tiers]
    extra = sorted({tier_of[s['id']] for s in roster} - set(known))  # e.g. 'evo' with no base form
    head = ('| Species | Stage | ' + ' | '.join(SLOTS) + ' | variant |\n|---|---|'
            + '---|' * (len(SLOTS) + 1))
    out, done_all = [], 0
    for tid in known + extra:
        members = [s for s in roster if tier_of[s['id']] == tid]
        if not members:
            continue
        rows, done = [], 0
        for s in members:
            d = os.path.join(theme, 'sprites', s['id'])
            cells = [f'✅ {frames(p)}f' if os.path.exists(p := os.path.join(d, slot + '.png')) else '·'
                     for slot in SLOTS]
            cells.append('✅' if os.path.exists(variant_idle(theme, s)) else '·')
            done += os.path.exists(os.path.join(d, 'idle.png'))
            stage = 'evolved' if s.get('rarity') == 'evo' else 'base'
            rows.append(f"| `{s['id']}` | {stage} | " + ' | '.join(cells) + ' |')
        done_all += done
        out += [f'**{tid} {done}/{len(members)} with art**', '', head, *rows, '']
    return '\n'.join(out), done_all, len(roster)


def item_rows(theme):
    got = [i for i in ITEMS if os.path.exists(os.path.join(theme, 'items', i + '.png'))]
    miss = [i for i in ITEMS if i not in got]
    return got, miss


def report(theme):
    section, done, total = species_section(theme)
    got, miss = item_rows(theme)
    bgs = sorted(f[:-4] for f in os.listdir(os.path.join(theme, 'backgrounds')) if f.endswith('.png')) \
        if os.path.isdir(os.path.join(theme, 'backgrounds')) else []
    lines = [
        BEGIN,
        f'**Species with art: {done}/{total}**, grouped by rarity tier (rarest first). `·` = missing: '
        'the engine falls back to a procedural animation of idle, or the placeholder sprite if idle '
        'is missing too. `Nf` = frames in the strip. `variant` = rare-variant sprites made '
        '(`tools/variants.py`).',
        '',
        section,
        f"**Items ({len(got)}/{len(ITEMS)}):** {', '.join(got) or 'none'}",
        f"**Missing items (placeholder used):** {', '.join(miss) or 'none'}",
        f"**Backgrounds:** {', '.join(bgs) or 'none yet (procedural sky + flat ground)'}",
        END,
    ]
    return '\n'.join(lines)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if len(args) != 1:
        sys.exit(__doc__)
    theme = theme_dir(args[0])
    text = report(theme)
    print(text)
    if '--write' in sys.argv:
        path = os.path.join(theme, 'README.md')
        body = open(path).read() if os.path.exists(path) else ''
        if BEGIN in body and END in body:
            body = body[:body.index(BEGIN)] + text + body[body.index(END) + len(END):]
        else:
            body = body.rstrip() + '\n\n## Art status\n\n' + text + '\n'
        open(path, 'w').write(body)
        print(f'\nupdated {os.path.relpath(path, ROOT)}')


if __name__ == '__main__':
    main()
