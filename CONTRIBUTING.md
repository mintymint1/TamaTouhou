# Contributing

Thanks for helping! Bug reports, play-testing notes, code, new themes and art are all welcome.

- [Reporting bugs](#reporting-bugs)
- [Ideas](#ideas)
- [Code](#code)
- [Themes and art](#themes-and-art)
- [Pull requests](#pull-requests)

## Reporting bugs

Open an [issue](https://github.com/mintymint1/TamaTouhou/issues/new/choose) with:

- what happened, and what you expected;
- the steps to make it happen again;
- **on the board:** the firmware version (the first line on the serial console at boot: `make monitor`, or any serial terminal at 115200), and how you installed it;
- **in the simulator:** your OS;
- a photo, screenshot or serial log if you have one.

A save file helps with game-logic bugs: export it (simulator `X`, board `tools/board_save.py export`) and attach the `.tamasave`.

## Ideas

Open an issue first for anything bigger than a small fix, so we can agree on it before you spend time on it. Some things are settled:

**What this game is not**

- **No battles.** No attack/defence stats, training, fights or battle screens. Weight, care and the play minigame stay.
- **One active pet.** Stored creatures are frozen. Caring for several at once is a different game.
- **No theme words in the engine.** Species, places, items and flavour text belong in a theme pack.
- **No grind loops.** A new egg is earned by raising a pet (see the store rules in [design.md](docs/design.md#42-rules)). Changes that let players farm eggs, medals or bond won't be merged.

## Code

Setup and the full tour: [docs/development.md](docs/development.md). In short:

```bash
make setup && make run     # play
make check                 # what CI runs
```

**Rules of the road**

1. **The engine (`firmware/lib/engine/`) stays hardware-free.** No Arduino, SDL, display or I2C calls: time comes in as arguments, saves go through `KvStore`. That's what keeps the tests and the simulator honest.
2. **Every rule change comes with a host test** in `firmware/test/host/`. Balance changes: rerun the playthrough tests (`make -C firmware/test/host FILTER=playthrough`, which prints the days to a full collection) and mention the new pacing in the PR.
3. **Keep `make run` working.** Look at your change in the simulator; for UI changes, attach a screenshot (the simulator can take them headless, see development.md).
4. **Screens draw only through `gfx->`** (rotation depends on it) and get one module each in `firmware/lib/app/`.
5. **Board code** (`firmware/src/`): read the hardware notes in development.md first; most of them were found the hard way.
6. **Save format:** changing what's saved means bumping the save version and migrating old saves, with a test. Never rename or reuse a species id.
7. **Generated files** (`firmware/lib/theme_gen/`) are committed: run `make data` after a theme change and commit the result. CI fails if they're out of date.

**Style:** C++17, 2-space indent, `camelCase` functions, `kConstants`, `s_`/`trailing_` members as in the surrounding code. Comments explain *why*, and say which bug a fix is for. Keep warnings at zero (`-Wall -Wextra`). Python: standard library + Pillow/PyYAML, `argparse` CLIs with a usage docstring.

## Themes and art

New themes and better art are very welcome. The format and a step-by-step guide are in [docs/themes.md](docs/themes.md).

- **License:** art you contribute is published under **CC BY-NC 4.0**, like the rest of the art ([themes/LICENSE](themes/LICENSE)). Only submit art you made yourself, or that's under a license that allows this; say who should be credited.
- **No ripped assets.** Don't include sprites, backgrounds or music extracted from games (ZUN's fan-work guidelines forbid it for Touhou, and most other rights holders do too).
- **AI-generated art:** say so in the PR, and which tool. The current Touhou sprites are AI-generated and hand-drawn replacements are especially welcome.
- **Fan themes:** keep them free and non-commercial, and respect the original creator's fan-work rules.
- Before opening the PR, run `tools/sprite_qa.py <theme> --sheet` and `tools/theme_status.py <theme> --write`, and check the result in `make run THEME=<theme>` (keys `N`/`P` page through every species, `V` shows the rare colour).

## Pull requests

1. Fork, branch from `main`, keep the change focused.
2. `make check` passes, and `make run` still works.
3. Fill in the PR checklist.
4. CI builds the tests (with sanitizers), the simulator and the board firmware on every PR.

By contributing you agree that your code is released under the [MIT license](LICENSE) and your art under [CC BY-NC 4.0](themes/LICENSE).
