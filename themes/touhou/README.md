# Touhou Project theme

The default theme: 48 species from Gensokyo. Animals and abandoned objects awaken into youkai (a KITTEN becomes CHEN or ORIN, an OLD UMBRELLA becomes KOGASA), with rarer single-form characters up to the two mythic protagonists, Reimu and Marisa. Every species has a rare colour (ALT COLOR).

Format: [docs/themes.md](../../docs/themes.md). Art license: [CC BY-NC 4.0](../LICENSE).

**About the art:** these sprites were generated with an AI pixel-art tool (PixelLab), then sliced, cleaned, recoloured for the rare colours and checked with this project's tools. They're a first version; hand-drawn replacements are welcome (see [CONTRIBUTING.md](../../CONTRIBUTING.md#themes-and-art)). Nothing in the engine depends on the current art: new strips just drop into the same folders.

| Path | What |
|---|---|
| `theme.yaml` | vocabulary, rarity tiers (common, rare, legendary, mythic), palette, habitats, starters |
| `roster.yaml` | the 48 species, their evolution lines, habitats and rare-colour rules |
| `strings/en.yaml` | flavour text ("Peaceful~", "INCIDENT!", "SPELL CARD GET!") |
| `sprites/<id>/` | animation strips + `anim.yaml`; `variant/` = generated rare-colour strips |
| `items/` | the egg (yin-yang orb) stages, power/point/faith items, dango, mess, icons, play object |
| `backgrounds/` | 9 habitat scenes, 232 × 156, drawn at 2× |

## Art status

<!-- STATUS:BEGIN (tools/theme_status.py) -->
**Species with art: 48/48**, grouped by rarity tier (rarest first). `·` = missing: the engine falls back to a procedural animation of idle, or the placeholder sprite if idle is missing too. `Nf` = frames in the strip. `variant` = rare-variant sprites made (`tools/variants.py`).

**mythic 2/2 with art**

| Species | Stage | idle | walk | sleep | eat | sad | happy | variant |
|---|---|---|---|---|---|---|---|---|
| `reimu` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `marisa` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |

**legendary 7/7 with art**

| Species | Stage | idle | walk | sleep | eat | sad | happy | variant |
|---|---|---|---|---|---|---|---|---|
| `sakuya` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `remilia` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `flandre` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `yuyuko` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `yukari` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `satori` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `koishi` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |

**rare 5/5 with art**

| Species | Stage | idle | walk | sleep | eat | sad | happy | variant |
|---|---|---|---|---|---|---|---|---|
| `meiling` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `patchouli` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `alice` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `sanae` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `nitori` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |

**common 34/34 with art**

| Species | Stage | idle | walk | sleep | eat | sad | happy | variant |
|---|---|---|---|---|---|---|---|---|
| `ice_fairy` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `cirno` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `tanned_cirno` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `kitten` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `chen` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `orin` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `fox_kit` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `ran` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `crow_chick` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `aya` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `okuu` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `rabbit` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `tewi` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `reisen` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `tanuki` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `mamizou` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `umbrella` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `kogasa` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `old_doll` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `medicine` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `sparrow` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `mystia` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `firefly` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `wriggle` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `mouse` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `nazrin` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `wolf_pup` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `kagerou` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `noh_mask` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `kokoro` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `darkness` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `rumia` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `phantom` | base | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |
| `youmu` | evolved | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ 4f | ✅ 6f | ✅ |

**Items (14/14):** egg_0, egg_1, egg_2, mess, emote, icon_feed, icon_play, icon_sleep, icon_clean, food_0, food_1, food_2, treat, play_object
**Missing items (placeholder used):** none
**Backgrounds:** bamboo_forest, forest_of_magic, former_hell, hakurei_shrine, human_village, misty_lake, netherworld, scarlet_mansion, youkai_mountain
<!-- STATUS:END -->

Marisa Kirisame's Mini-Hakkero.