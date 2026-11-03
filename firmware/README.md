# firmware/

The game: the engine and screens shared by every target, the ESP32-S3 board code, the PC simulator and the host tests. Build everything from the repository root (`make run`, `make test`, `make board`).

| Folder | What |
|---|---|
| `lib/engine/` | game rules, hardware-free C++17 |
| `lib/app/` | the screens, shared by board and simulator |
| `lib/theme_gen/` | generated from `themes/<id>/` (don't edit) |
| `src/` | ESP32-S3 drivers and the board's main loop |
| `sim/` | the PC simulator (SDL3) |
| `test/host/` | unit tests |

Details: [docs/development.md](../docs/development.md).
