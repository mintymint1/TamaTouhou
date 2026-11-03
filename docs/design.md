# Game design: navigation, Status, storage, progression, saves

The reference for how the game is meant to work. Player-facing rules: [how-to-play.md](how-to-play.md). Theme words are examples from the Touhou pack; the engine uses neutral terms (`storage`, `keepsake`, `milestone`).

---

## 1. Goals

1. **Swipes open new things.** The main screen becomes a hub; each swipe direction opens a screen.
2. **See your stats and how far you are from the next milestone** (next level, evolution, streak, bond, medals, collection unlocks), with progress bars and time estimates.
3. **Keep what evolves.** When a pet evolves, its previous form is kept in a **storage** the player can browse. Any stored creature can be made the active pet, and the active pet can go into storage ("catch them all").

Non-goals: battles, trading between devices.

---

## 2. Navigation: the swipe hub

```
                    ▲ swipe UP
                 STATUS (stats + milestones)
                    │
   ◄ swipe LEFT     │      swipe RIGHT ►
   STORAGE  ──── MAIN (pet) ────  CHRONICLE
   ("the Gap")      │              (collection log)
                    │
                    ▼ swipe DOWN
                 SETTINGS (clock, sound, brightness, language)
```

| Rule | Why |
|---|---|
| **Enter with a swipe, leave with the opposite swipe** (or tap the ⌃ back mark at the top of the circle) | One mental model for every screen |
| **Inside a screen, page along the other axis**: Status (entered vertically) pages **left/right**; Storage and Chronicle (entered horizontally) page **up/down** | Paging never collides with "go back" |
| Page dots on the bottom arc; the screen title on the top arc | Fits the round display; nothing in the corners |
| No navigation during evolution, ending ceremonies, dialogs or the play minigame | Big moments can't be swiped away |
| Any screen returns to Main after 30 s without touch | AMOLED burn-in, and the pet stays the default view |
| **First-run hints:** small labelled arrows at the four edges of Main; each disappears once its screen has been opened | Discoverability without a manual |

Gesture thresholds stay as implemented (`gesture.h`: 80 px, under 800 ms). A tap on the pet is still a caress, and a long-press is still release.

---

## 3. Status screen (swipe up): stats and "next milestone"

### 3.1 Pages (swipe left/right)

| # | Page | Contents |
|---|---|---|
| 1 | **Next up** *(default)* | The **3 nearest milestones**, each with a progress bar, "x / y" and an ETA. Example: `Lv 13 → 14 · 42 min` · `AWAKEN at Lv 16 · ~2 h 40 m · all needs ≥ 40 ✓` · `Streak 5 → 7 days · 2 days` |
| 2 | **Care** | The 4 needs with numbers and per-minute drain, mood (*Peaceful* / *Hungry*…), weight, mess, and **care mistakes**, with the note "each one delays awakening by 1 level" |
| 3 | **Growth** | Level, age, the **evolution line** (current form → next form as a silhouette, or "???" if not yet collected; both branches for kitten/crow/rabbit), level needed incl. mistake delay, and the farewell timer for final forms ("ready to explore in 1 d 4 h") |
| 4 | **Bond & streak** | Bond 0–100 with today's cap (x / 20), streak and best streak, the 8 medals (earned in colour, others greyed) with a hint for the next one |
| 5 | **Collection** | Chronicle x / 48, alt colours found, storage x / 96, and **tier unlocks** from `rarity_tiers` ("Legendary eggs: 7 / 10 collected", "Mythic: 7 / 30"), plus the current egg-odds bonus from streak and bond |

An egg shows a single page: hatch progress (taps / minutes) and its rarity hint.

### 3.2 Milestones (engine-defined, theme-worded)

The engine computes a list of `Milestone {kind, current, target, unit, eta_minutes, blocked_reason}`. The UI picks the nearest 3 by ETA (or by % done when there's no ETA).

| Kind | Target | ETA from |
|---|---|---|
| Next level | level + 1 | minutes per level |
| Evolution | evolve level + care mistakes | levels × 60 min; blocked reasons: "a need is below 40", "asleep" |
| Farewell eligible | final form + `farewell_age_days` | age |
| Streak | next of 3 / 7 / 30 / 100 days | days |
| Bond | 100 | today's cap remaining |
| Next medal | closest of the 8 (Lv 10/25/50, favourite food, 7-day streak, max bond, final form, fit) | per medal |
| Tier unlock | each `rarity_tiers[].unlock_collected` | collected count |
| Living collection | storage 25 / 50 / 75 / 100 % | count |

Reaching a milestone fires the existing `EV_MILESTONE` banner and sound; text comes from the theme (e.g. "SPELL CARD GET!").

### 3.3 Main-screen glance (optional setting)

A thin **progress arc around the round bezel** for the #1 "next up" milestone. It suits the circular screen and needs no extra space. Off by default if it adds clutter.

---

## 4. Storage (swipe left): the living collection

Engine term: **storage**. Touhou wording: **"THE GAP"** (Yukari's gaps), theme vocabulary `storage`.

### 4.1 How creatures get into storage

| Way in | When | Notes |
|---|---|---|
| **Keepsake on evolution** | Every time the active pet evolves | A copy of its **previous form** goes into storage: kitten → Chen leaves a kitten behind. Same alt-colour flag; level = the level it evolved at; needs full; bond 0; no nickname; tagged "keepsake of <name>". |
| **Switch** | From a stored creature's detail page | The active pet goes into storage exactly as it is, and the chosen one becomes active. |
| **Store & start a new egg** | From Status page 1 or a menu | The active pet goes into storage and a new egg starts. It's a **neutral** ending (no farewell blessing, no runaway curse). Gated by raising, not by level (see rules and §7), so eggs can't be farmed. |

### 4.2 Rules

| Rule | Reason |
|---|---|
| **Stasis:** stored creatures don't age, get hungry, make mess or mistakes, and can't run away | Only the active pet needs care. That keeps the game a single-pet virtual pet, and the device simulates one pet. |
| **One per species × colour** (48 × normal + 48 × ALT COLOR = **96 max**) | "Catch them all" has a clear finish line; it also stops keepsake loops from flooding storage. Storing a second one asks "Replace the stored CHEN (Lv 22)?". |
| **Branch completion:** a keepsake that is made active again can evolve again, and the engine still prefers the branch you haven't collected | This is how you get both Chen **and** Orin from one kitten line, or Aya and Okuu. |
| No keepsake if that species+colour is already stored (or storage is full: show "Storage full") | No duplicates, no infinite kitten farm |
| **A keepsake only evolves into a form not collected yet in its own colour**. Once all its forms are collected it stays as it is (`evolutionsExhausted()`; Growth page: "All forms collected"). Hatched pets are unaffected. | Without it, evolve → switch the keepsake back → evolve looped forever, replaying evolutions and medals for free |
| **Store-and-new-egg gate**: since it hatched or came out of storage, the pet must have **evolved** (`NotAwakened` otherwise). A pet with nothing left to evolve into needs **9 levels' worth of time as the active pet** instead (`TooYoung`, `storeReadyInMinutes()`). Also not during an egg, dialog, evolution or ceremony. | A new egg is earned by raising. The old Lv-10 rule let you switch a stored final form in, store it, and get a free egg, over and over. |
| An **egg can't be stored or switched out**; while the active slot holds an egg, Storage is view-only ("Hatch the egg first") | Keeps egg/rarity logic unchanged |
| **Endings unchanged:** farewell (blesses the next egg), runaway (curses it), release (neutral) still remove the active pet; storage is untouched | Farewell stays a meaningful choice: bless the next egg, or keep the friend in storage |
| **Release from storage:** long-press a stored creature, then confirm | Frees a slot; no ending effects |
| Player-wide progress (streak, Chronicle, total medals) is shared; bond, medals, nickname and needs belong to each creature | Matches the current per-pet/player split |

### 4.3 Screen layout (round 466 × 466)

- **Grid page:** a 3 × 3 grid of 40 px thumbnails at 2× inside the circle. Owned creatures only, sorted by tier (mythic first) then roster order. ALT COLOR ones get a ★ badge. Title "THE GAP 14/96" on the top arc; swipe up/down to page.
- **Detail page** (tap a creature): its animated idle in its habitat, name/nickname, level, colour badge, origin ("keepsake of SPARKY" / "hatched" / "stored"), mini need bars, and a **SWITCH** button → dialog "Send KOGASA to the Gap and bring out KITTEN?" (both sprites shown) → switch effect.
- **Switch effect:** theme-optional sprite/colour (Touhou: a gap slit with eyes opening). The fallback is a procedural shrink-into-a-line + flash, so no new art is needed.

---

## 5. Chronicle (swipe right): collection log

- **Grid** of all 48 roster entries. Never seen = black silhouette; collected = sprite; in storage = small box badge; ALT COLOR found = ★. Swipe up/down to page.
- **Detail:** name, tier, habitat, evolution line with silhouettes, "In the Gap → view" link to the storage detail, and a normal / ALT COLOR toggle if both were found.
- Same art as storage (thumbnails derived from `idle` frame 0 by the builder).
- **Hold to switch**: **hold a creature for ~1 s** (in the grid or in its detail view) to make it your active pet. It's a shortcut to the Gap's SWITCH and uses the same confirmation dialog, so nothing changes without a yes.

  | Holding… | Result |
  |---|---|
  | A species that's in the Gap | The switch dialog "Send <active> to the Gap and bring out <held>?" → yes = `switchWith(slot)` → the switch effect → back on Main with the new pet |
  | Both colours are stored | Grid: the colour shown in the cell (normal unless only the ALT COLOR is stored). Detail: the colour currently toggled. |
  | Your active pet's own species + colour | Hint "<name> is by your side" (nothing to switch) |
  | Collected but not in the Gap (it left through farewell/release, or was never kept) | Hint "Not in the Gap" |
  | Never seen (silhouette) | Nothing |
  | Active slot is an egg / an ending or evolution is running | Same refusals as the Gap ("Hatch the orb first" / busy) |
  | The active pet's kind is already stored elsewhere | The Gap's replace dialog (`StoreError::Duplicate` → retry with replace) |

  The hold time on grid screens (Chronicle, the Gap) is **~1 s**; holding the pet on Main stays at **3 s** (release must never be accidental). In the Gap, a hold still means *release* (with its confirm dialog).

---

## 6. Settings (swipe down)

Clock, sound on/off, brightness, the bezel progress ring, and screen rotation. Same paging rules.

**Rotate screen:** on the board (and the simulator) a row turns the whole picture a quarter clockwise per tap: 0° → 90° → 180° → 270° → 0°. Handy when the board stands on another edge.
- **It's a device setting, not the pet's:** stored in the board's own NVS namespace `device` (key `rot`), outside the per-theme save, so exporting, importing or switching pets never turns the screen. Written once the taps stop (5 s without a touch, or screen off), because an NVS write pauses the game for up to a second.
- **The CO5300 can't rotate 90° in hardware** (only flips), so `Arduino_Canvas::setRotation()` turns everything as it's drawn. Rectangles stay rectangles, so it costs nothing per frame. Touches come back through `glassToScreen()` (`lib/app/gfx.h`), so taps and swipes follow the turned picture. `PcCanvas` copies the board canvas's mapping, and `test_rotation.cpp` checks both against each other and the touch inverse.

---

## 7. Progression: no repeats, then rare colours, then free choice

Engine rules in `lib/engine/pet_collection.cpp` (`rollEggSpecies`, `prepareEgg`) and `pet_storage.cpp` (`canStoreActive`), covered by `test/host/test_progression.cpp`.

### 7.1 Eggs never repeat
- An egg only rolls a **base form not collected yet in the egg's colour**. Normal colour and rare colour (ALT COLOR) are **separate collections**: a rare-colour pet fills only the rare entry.
- **One exception, so the roster can't dead-end:** a base form you *lost* before its line was finished (released, ran away, released from storage) can come back, but only if **nothing you still own can evolve into the missing forms**. If you still own Cirno, a lost Ice Fairy doesn't come back, because Cirno can still become Tanned Cirno.
- If the rolled tier has nothing left, the egg comes from the **nearest tier that does**: more common first, then rarer. A tier lock only delays a tier; it never blocks the last species. So Reimu and Marisa end up among the last, and once they're all that's left, every egg is one of them.

### 7.2 Three phases
| Phase | When | Eggs |
|---|---|---|
| **Normal** | Until all species are collected in normal colour | Species missing in normal colour. A lucky roll (1/48 → 1/8, as before) makes it a rare colour, picked among the rare colours still missing. |
| **Variant** | All normal colours collected | **Every egg is a rare colour not collected yet**, with the same tier odds. Tier locks count the rare-colour collection. |
| **Complete** | Both collections complete (96/96 for Touhou) | **The player chooses**: any base form, either colour, on an egg-picker screen (`chooseEgg()`). No auto-hatch while choosing. |

Evolved forms come from evolving, not from eggs: a rare-colour base form evolves into a rare-colour youkai and leaves a rare-colour keepsake behind. A banner announces each new phase (`EV_PHASE`; strings `S_PHASE_VARIANT`, `S_PHASE_COMPLETE`).

### 7.3 The core loop
**Raise → evolve → store → new egg.** "Store & start a new egg" opens once the pet has evolved since it hatched or left storage (§4.2). For single-stage species (rare, legendary, mythic), and for stored creatures with nothing left to evolve into, it opens after 9 levels' worth of time as the active pet. Farewell stays the alternative for final forms 3+ days old: it blesses the next egg, but the creature doesn't go into storage.

### 7.4 Pacing (measured by the playthrough tests, Touhou roster)
| Player | Normal collection done | Both collections (96/96) |
|---|---|---|
| Perfect care (checks in every 5 min) | 17 days | 34 days |
| Realistic: checks in every 2 h, asleep 23:00–07:00 | 48 days | 90 days |

Each run hatches 58 eggs (29 base forms × 2 colours) with **zero repeats**. The realistic run's slowest pet collected 64 care mistakes. Every mistake delays evolution by a level, so long lines (Lv 30 kitsune) drag for a player who checks in rarely. If that feels too slow, the next knob to turn is capping how many levels mistakes can add.

`careMistakes` now saturates at 255. It used to wrap to 0 after about 10 days of neglect, which made evolution instantly available. The playthrough test found this.

---

## 8. Save export / import

A savegame (pet + storage + collection) moves between devices or into a backup as **one text file**, `*.tamasave`:

```
TAMA-SAVE 1 touhou
<base64 of the package: pet blob + storage blob + CRC32>
```

| Where | Export | Import |
|---|---|---|
| Simulator (`make run`) | **X** → `<save dir>/export.tamasave` | **Shift+I** (that file), or `--import FILE` at start |
| Board | Serial console `SAVE` (one line) | `LOAD <text>`. Or `tools/board_save.py export|import FILE` over USB |

- **The same file works everywhere** as long as the theme matches (the header names it; a wrong theme is refused by name).
- **Import validates everything before it changes anything:** tag, version, theme, CRC32, and both blobs. A damaged or foreign file leaves the current game untouched. Then it persists both blobs and the screens start over on the imported game (`EV_LOADED`).
- **Time keeps passing:** after an import, the time since the export is applied like time powered off (needs drop with mercy, capped at 14 days, no evolution or runaway while away).
- Engine API: `Pet::exportSave()` / `importSave()` (binary package) + `lib/engine/save_text.h` (text form). Tests: `test/host/test_save_transfer.cpp` (round trip, both text layouts, refusals, v2 → v3 migration).

Save format v3 adds the egg-choice flag and the stint (minutes as the active pet, and whether it evolved). v1/v2 saves migrate: a pet raised from its egg counts its whole life as its stint, and an evolved form that hatched in your care counts as evolved. In v2, a rare colour also set the normal-colour bit, and those can't be told apart, so old saves may show a few normal entries that were only ever seen in rare colour.
