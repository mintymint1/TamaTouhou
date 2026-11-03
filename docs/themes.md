# Themes

The engine knows nothing about Touhou. Every creature, word, colour, place and picture comes from a **theme pack** in `themes/<id>/`. You can make a new theme without touching the C++.

```
themes/<id>/
  theme.yaml              manifest: name, vocabulary, rarity tiers, palette, habitats, starters, tunables
  roster.yaml             every species: rarity, habitat, evolution, rare-colour rule
  strings/en.yaml         optional: overrides of the engine's English text
  sprites/<species>/      animation strips per species (+ anim.yaml, + variant/ rare colours)
  items/                  egg, foods, treat, mess, icons, play object
  backgrounds/<habitat>.png
  README.md               what art exists (tools/theme_status.py --write)
```

`tools/build_theme.py <id>` validates the pack, writes the C++ tables in `firmware/lib/theme_gen/`, and with `--pack` converts the art into `firmware/data/*.spr` for the board and the simulator. `make data THEME=<id>` runs it, and `make run THEME=<id>` plays it. The theme is compiled into the firmware, so switching themes means a new build.

## Engine words

Code, saves and these docs use neutral words; the theme renames them on screen.

| Engine word | Meaning | Touhou says |
|---|---|---|
| egg | what a new pet hatches from | YIN-YANG ORB |
| species | a kind of creature, with a stable `id` | |
| evolution | a species turning into another | AWAKEN |
| variant | the rare colour of a species | ALT COLOR |
| collection | every species × colour you've had | GENSOKYO CHRONICLE |
| storage | where kept creatures wait | THE GAP |
| keepsake | the previous form kept when a pet evolves | KEEPSAKE |
| needs | food / joy / energy / hygiene | FOOD / FUN / POWER / TIDY |
| foods, treat | three foods and one treat | POWER/POINT/FAITH ITEM, DANGO |
| habitat | a species' home background | |

## `theme.yaml`

```yaml
id: touhou                     # [a-z0-9_]; saves are namespaced by it
name: "Touhou Project"
version: 1
engine_api: 1

vocabulary:
  egg: "YIN-YANG ORB"
  collection: "GENSOKYO CHRONICLE"
  variant: "ALT COLOR"
  evolve_verb: "AWAKEN"
  needs: [FOOD, FUN, POWER, TIDY]          # food / joy / energy / hygiene
  foods: ["POWER ITEM", "POINT ITEM", "FAITH ITEM"]
  treat: "DANGO"
  clean_action: "BOMB!"
  storage: "THE GAP"
  keepsake: "KEEPSAKE"

# Rolled rarest first; the first that hits wins. odds are percentages, blessed_odds apply after
# a farewell, unlock_collected = species collected before the tier can roll. The last tier
# catches the rest.
rarity_tiers:
  - { id: mythic,    odds: 1.0, blessed_odds: 2.5, unlock_collected: 30, hint: "PROTAGONIST?!" }
  - { id: legendary, odds: 4.0, blessed_odds: 10,  unlock_collected: 10, hint: "LEGENDARY" }
  - { id: rare,      odds: 25,  blessed_odds: 40,  unlock_collected: 0,  hint: "RARE" }
  - { id: common,    odds: rest }

palette:                       # UI colours; anything left out gets a neutral default
  bg_day: "#f4ecdc"
  bg_night: "#14111f"
  ink: "#3a2626"
  ink_night: "#e8dcf0"
  bar_ok: "#4caf50"
  bar_warn: "#f2b632"
  bar_bad: "#d8283a"
  track: "#ddd0bb"
  accent_default: "#c8283a"
  clean_fx: "#ffffff"

habitats:                      # backgrounds/<id>.png; ground_y = where the feet go
  - { id: hakurei_shrine, sky: none, ground_y: 304 }

tunables:
  variant_odds: 48             # 1 in N eggs is a rare colour (before bonuses)
  farewell_age_days: 3         # how old a final form must be to offer a farewell

starters: [ice_fairy, kitten, umbrella]   # three base forms offered in the very first game
```

## `roster.yaml`

```yaml
- id: kitten                   # stable [a-z0-9_]; saves store its hash: never rename or reuse it
  name: "KITTEN"               # up to 14 characters (uppercase suits the pixel font)
  rarity: common               # a tier id, or evo for forms only reachable by evolving
  habitat: human_village
  accent: "#a0704a"            # name colour in the header
  favourite_food: 1            # optional, 0-2 (default: derived from the id)
  variant: { hue_shift: 180 }  # how its rare colour is made (see below)
  evolves:                     # several edges = a branch; the engine picks the uncollected one
    - { to: chen, level: 16 }
    - { to: orin, level: 16 }
- { id: chen, name: "CHEN", rarity: evo, habitat: youkai_mountain, accent: "#d0403a", variant: { hue_shift: 150, protect_skin: true } }
```

The builder refuses a pack when:

- an id is duplicated, or two ids hash to the same 16-bit key (rename one);
- an evolution points at an unknown species, or there's a cycle;
- an evolution target isn't `rarity: evo`, or an `evo` species can't be reached;
- a rarity isn't a declared tier, or a tier has no base forms;
- a habitat doesn't exist, a name is longer than 14 characters, or `favourite_food` isn't 0–2;
- a starter isn't a base form, or only some tiers use `odds: rest`.

## Sprites

One folder per species: `sprites/<id>/`. Each animation is a **horizontal PNG strip** (frames left to right, all the same size, transparent background).

| Slot | File | Facing | Needed? | Used for | If it's missing |
|---|---|---|---|---|---|
| idle | `idle.png` | front | **yes** | standing; thumbnails; silhouettes | (the species shows a placeholder) |
| walk | `walk.png` | **right** | recommended | wandering (mirrored for left) | idle sliding sideways with a bob |
| sleep | `sleep.png` | front | recommended | sleeping | idle, dimmed, slow breathing, "Zz" |
| eat | `eat.png` | front | recommended | after feeding | idle at double speed with a chew bob |
| sad | `sad.png` | front | recommended | a need under 25; runaway | idle at half speed, lower, darker |
| happy | `happy.png` | front | recommended | cleaning, petting, favourite food | idle with a hop |
| flair1..3 | `flair1.png`… | front | optional | random idle variety | not played |

So **one idle strip per species is already playable**; everything else makes it nicer.

**Pixel art at native size.** The engine scales by a whole number so the idle pose is about 170 px tall on screen: aim for **32–56 px** of idle content (big final forms up to ~80 px). Keep a species to about 16 colours (255 at most). Animations loop, so the last frame should flow into the first.

### `anim.yaml` (optional)

```yaml
frame: [92, 92]              # frame size; default: square, by the strip's height
idle:  { fps: 5 }            # or ms: 200 (one duration for every frame)
walk:  { fps: 10 }
sleep: { fps: 2, frame: [104, 104] }   # per-slot frame size overrides the global one
happy: { fps: 10, mode: pingpong }     # forwards then backwards (hides a jump at the loop)
```

Defaults: idle 200 ms per frame, walk 120, sleep 500, eat 150, sad 250, happy 120. `sleep` and `sad` play ping-pong unless `mode: loop` says otherwise.

### Rare colours (variants)

A rare colour is the same pixels with a different palette, so it needs no new art. Set the rule per species in `roster.yaml` and run `tools/variants.py <id>`; it writes `sprites/<species>/variant/`.

| Key | Effect |
|---|---|
| `hue_shift: <degrees>` | Rotate the hue of coloured pixels (greys and blacks stay, so outlines survive) |
| `hue_range: [lo, hi]` | Only shift hues in this range |
| `protect_skin: true` | Leave skin tones alone. **Use it on every humanoid**, or faces turn green |
| `palette_map: {"#rrggbb": "#rrggbb"}` | Exact recolours (shading is kept). `variants.py <id> --palette <species>` lists a sprite's colours |
| `sprites: variant/` | Hand-made rare-colour strips, for when the shape changes too |

Generated `variant/` folders contain a `.generated` marker; a `variant/` folder without it counts as hand-made and is never overwritten.

## Items

| File | Size | Notes |
|---|---|---|
| `egg_0.png`, `egg_1.png`, `egg_2.png` | ~32 × 32 | intact, cracked, almost open; drawn at 5× |
| `mess.png` | 16–24 px | up to three on the ground |
| `emote.png` | 16–24 px | the "likes it" heart |
| `icon_feed.png`, `icon_play.png`, `icon_sleep.png`, `icon_clean.png` | 16 × 16 | button icons, drawn at 2× |
| `food_0.png`, `food_1.png`, `food_2.png`, `treat.png` | 16 × 16 | food picker, drawn at 3× |
| `play_object.png` | 16–24 px | what you keep in the air in the minigame |

## Backgrounds

`backgrounds/<habitat>.png`, **232 × 156**, fully opaque, drawn at 2× into the 466 × 312 scene above the panel. A habitat without a picture gets a plain sky and ground in the palette colours. The engine dims and tints it at night and while the pet sleeps.

## Text

The engine has a fixed set of text IDs with neutral English defaults (`S_HUNGRY = "Hungry..."`). `strings/en.yaml` overrides any of them, with placeholders filled in at runtime:

```yaml
S_HAPPY: "Peaceful~"
S_MEDAL_BANNER: "SPELL CARD GET!"
S_EVO_Q: "{name} wants to {evolve_verb}"
S_KEEPSAKE_GOT: "{name} left a keepsake in the Gap"
```

Placeholders: `{egg}`, `{collection}`, `{evolve_verb}`, `{storage}`, `{name}`, `{other}`, `{n}`, `{max}`. Look at `themes/touhou/strings/en.yaml` for the full list in use. The pixel font covers ASCII only.

## Making a new theme

1. Copy the Touhou pack: `cp -r themes/touhou themes/<new-id>` and set `id` and `name` in `theme.yaml`.
2. Write `roster.yaml`: base forms in each tier, evolution edges, three starters, habitats.
3. Fill in `vocabulary`, `palette` and `strings/en.yaml`.
4. Replace the art: at least `idle.png` per species, plus the items. Delete sprite folders you don't use.
5. `make data THEME=<new-id>` and fix what the validator reports.
6. `.venv/bin/python tools/variants.py <new-id>` for the rare colours.
7. `.venv/bin/python tools/sprite_qa.py <new-id> --sheet` checks slicing, transparency, feet, facing and loop pops, and writes a contact sheet to `themes/<new-id>/art/qa_sheet.png`.
8. `make run THEME=<new-id>`. Press `N`/`P` to page through every species.
9. `.venv/bin/python tools/theme_status.py <new-id> --write` updates the theme's README table.

Want it in the main project? Open a pull request; see [CONTRIBUTING.md](../CONTRIBUTING.md#themes-and-art) for the art rules.
