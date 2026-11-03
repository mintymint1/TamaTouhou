// PC simulator: the real game engine + screens in a 466x466 window, so the game can be
// play-tested without the board. Mouse = finger (click = tap, drag = swipe, hold 3 s = long
// press). Same rules (lib/engine), same screens (lib/app), same art (firmware/data/).
//
//   make run              pack the theme art, build, start
//   ./build/tamatouhou-sim [--zoom 2] [--speed 1|2|3] [--data DIR] [--save DIR] [--import FILE]
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "version.h"
#include "app_platform.h"
#include "game.h"
#include "gesture.h"
#include "gfx.h"
#include "pet.h"
#include "platform_pc.h"
#include "save_file.h"
#include "theme_data.h"
#include "ui.h"

#ifndef DATA_DIR
#define DATA_DIR "data"
#endif
#ifndef SAVE_DIR
#define SAVE_DIR "save"
#endif

Gfx *app::gfx = nullptr;

// --------------------------------------------------------------------------- main

static void help() {
  std::printf(
      "\nmouse: click = tap, drag = swipe, hold 3 s on the pet = release dialog\n"
      "arrows: swipe (up = Status, left = Storage, right = Chronicle, down = Settings; opposite = back)\n"
      "keys:  1/2/3 game speed (real time / 1 s per game minute / 0.1 s per minute)\n"
      "       H +1 game hour   L +1 level   D next day (streaks)   F fill needs   E empty needs\n"
      "       N/P next/prev species   V toggle rare variant (shiny)   R make it ready to run away\n"
      "       G roll 20 eggs   S stats   M mute   Shift+W wipe save (new game)   Q/Esc quit\n"
      "       B fill storage   K evolve now (makes a keepsake)\n"
      "       X export the save to <save dir>/export.tamasave   Shift+I import it back\n\n");
}

int main(int argc, char **argv) {
  int zoom = 1, speed = 1;
  std::string dataDir = DATA_DIR, saveDir = SAVE_DIR;
  std::string shot, keys, taps;   // headless screenshot mode (testing / docs)
  std::string importPath;         // --import: replace the save with a .tamasave file at start
  uint32_t shotMs = 1500;
  for (int i = 1; i + 1 < argc; i += 2) {
    std::string a = argv[i];
    if (a == "--zoom") zoom = std::atoi(argv[i + 1]);
    else if (a == "--speed") speed = std::atoi(argv[i + 1]);
    else if (a == "--data") dataDir = argv[i + 1];
    else if (a == "--save") saveDir = argv[i + 1];
    else if (a == "--shot") shot = argv[i + 1];          // save a BMP after --shot-ms and exit
    else if (a == "--shot-ms") shotMs = (uint32_t)std::atoi(argv[i + 1]);
    else if (a == "--keys") keys = argv[i + 1];          // e.g. "nnv": shortcut keys at start
    else if (a == "--taps") taps = argv[i + 1];          // "233,200;U;L;H233,200": taps, swipes U/D/L/R, holds
    else if (a == "--import") importPath = argv[i + 1];
  }
  if (zoom < 1) zoom = 1;
  pc::setDataDir(dataDir);

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  std::string title = std::string("TamaTouhou sim ") + TAMA_VERSION + " - " + tama::kTheme.id;
  SDL_Window *win = SDL_CreateWindow(title.c_str(), kScreenW * zoom, kScreenH * zoom,
                                     SDL_WINDOW_HIGH_PIXEL_DENSITY | (shot.empty() ? 0 : SDL_WINDOW_HIDDEN));
  SDL_Renderer *ren = SDL_CreateRenderer(win, nullptr);
  if (!win || !ren) {
    std::fprintf(stderr, "window: %s\n", SDL_GetError());
    return 1;
  }
  SDL_SetRenderVSync(ren, 1);
  SDL_SetRenderLogicalPresentation(ren, kScreenW, kScreenH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, kScreenW, kScreenH);
  SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);

  // Round panel: everything outside r = 233 is black glass, like the real AMOLED.
  SDL_Texture *mask = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, kScreenW, kScreenH);
  {
    std::vector<uint32_t> m((size_t)kScreenW * kScreenH);
    for (int y = 0; y < kScreenH; y++)
      for (int x = 0; x < kScreenW; x++) {
        float dx = x - CX + 0.5f, dy = y - CY + 0.5f;
        m[(size_t)y * kScreenW + x] = (dx * dx + dy * dy > 233.0f * 233.0f) ? 0xFF000000u : 0x00000000u;
      }
    SDL_UpdateTexture(mask, nullptr, m.data(), kScreenW * 4);
    SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(mask, SDL_SCALEMODE_NEAREST);
  }

  pc::audioOpen();

  PcCanvas canvas(kScreenW, kScreenH);
  app::gfx = &canvas;
  canvas.onFlush = [&]() {
    SDL_UpdateTexture(tex, nullptr, canvas.pixels(), kScreenW * 2);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, nullptr, nullptr);
    SDL_RenderTexture(ren, mask, nullptr, nullptr);
    SDL_RenderPresent(ren);
  };

  pc::Game game(saveDir);
  tama::Pet &pet = game.pet();
  const uint32_t speeds[4] = {60000, 60000, 1000, 100};
  pet.setTickMs(speeds[speed >= 1 && speed <= 3 ? speed : 1]);

  long dayOffset = 0;
  game.epoch = [&]() { return (uint32_t)(plat::epoch() + dayOffset * 86400L); };
  auto ticks = []() { return (uint32_t)SDL_GetTicks(); };
  game.begin(ticks());
  std::printf("theme %s: %u species, data %s, save %s\n", tama::kTheme.id, tama::kTheme.speciesCount,
              dataDir.c_str(), game.saveDir().c_str());
  help();
  const std::string exportPath = saveDir + "/export." + pc::kSaveFileExt;
  auto importFrom = [&](const std::string &path) {
    std::string err;
    if (pc::importSaveFile(pet, path, ticks(), game.epoch(), err)) std::printf("imported %s\n", path.c_str());
    else std::printf("import failed: %s\n", err.c_str());
  };
  if (!importPath.empty()) importFrom(importPath);

  bool running = true;
  auto handleKey = [&](SDL_Keycode k, bool shift) {
    uint32_t t = ticks();
    switch (k) {
      case SDLK_ESCAPE: case SDLK_Q: running = false; break;
      case SDLK_1: case SDLK_2: case SDLK_3:
        pet.setTickMs(speeds[k - SDLK_0]);
        std::printf("speed: 1 game minute = %u ms\n", pet.tickMs());
        break;
      case SDLK_H: pet.advanceMinutes(60); std::printf("+1 hour (level %u)\n", pet.level()); break;
      case SDLK_L: pet.debugSetLevel(pet.level() + 1); std::printf("level %u\n", pet.level()); break;
      case SDLK_D:
        dayOffset++;
        pet.noteEpoch(game.epoch());
        std::printf("next day (+%ld)\n", dayOffset);
        break;
      case SDLK_F: pet.debugSetNeeds(100, 100, 100, 100); break;
      case SDLK_E: pet.debugSetNeeds(0, 0, 0, 0); break;
      case SDLK_R: pet.debugRunawayReady(); break;
      case SDLK_V: pet.debugToggleVariant(); std::printf("variant %d\n", pet.variant()); break;
      case SDLK_N: case SDLK_P: {
        int n = tama::kTheme.speciesCount;
        int cur = pet.isEgg() ? -1 : pet.species();
        int next = ((cur < 0 ? 0 : cur) + (k == SDLK_N ? 1 : n - 1)) % n;
        pet.debugSetSpecies((uint16_t)next);
        std::printf("species %s\n", tama::kTheme.species[next].id);
        break;
      }
      case SDLK_G: {   // 20 egg rolls in the colour being hunted now (no repeats, see pet.h)
        bool alt = pet.phase() == tama::Phase::Variant;
        for (int i = 0; i < 20; i++) {
          uint16_t r = pet.rollEggSpecies(alt);
          std::printf("%s%s ", alt ? "*" : "", r == tama::kNoSpecies ? "(none)" : tama::kTheme.species[r].id);
        }
        std::printf("\n");
        break;
      }
      case SDLK_S:
        std::printf("%s lv%u food %u joy %u energy %u hygiene %u mess %u weight %u mistakes %u bond %u streak %u collected %u/%u alt %u variant %d phase %d stint %u\n",
                    pet.isEgg() ? "egg" : pet.speciesDef()->id, pet.level(), pet.food(), pet.joy(), pet.energy(),
                    pet.hygiene(), pet.mess(), pet.weight(), pet.careMistakes(), pet.bond(), pet.streak(),
                    pet.collectedCount(), tama::kTheme.speciesCount, pet.variantCollectedCount(), pet.variant(),
                    (int)pet.phase(), pet.stintMinutes());
        break;
      case SDLK_UP: game.swipe(tama::Gesture::SwipeUp, t); break;   // arrow keys = swipes
      case SDLK_DOWN: game.swipe(tama::Gesture::SwipeDown, t); break;
      case SDLK_LEFT: game.swipe(tama::Gesture::SwipeLeft, t); break;
      case SDLK_RIGHT: game.swipe(tama::Gesture::SwipeRight, t); break;
      case SDLK_M: pc::setMuted(!pc::muted()); break;
      case SDLK_B: pet.debugFillStorage(); std::printf("storage %u/%u\n", pet.storageCount(), pet.storageCapacity()); break;
      case SDLK_K:   // evolve right now (fills needs, raises the level) to test keepsakes
        if (pet.isEgg() || !pet.speciesDef()->evoCount) {
          std::printf("can't evolve: %s\n", pet.isEgg() ? "it's an egg" : "final form");
        } else if (pet.evolutionsExhausted()) {
          std::printf("can't evolve: keepsake with all forms collected\n");
        } else {
          pet.debugSetNeeds(100, 100, 100, 100);
          while (!pet.canEvolve() && pet.level() < 120) pet.debugSetLevel(pet.level() + 1);
          uint16_t before = pet.species();
          pet.evolve(t);
          if (pet.species() != before) std::printf("evolved: %s (storage %u)\n", pet.speciesDef()->id, pet.storageCount());
          else std::printf("didn't evolve\n");
        }
        break;
      case SDLK_W:
        if (shift) { pet.factoryReset(t); ui::begin(pet); std::printf("save wiped: new game\n"); }
        break;
      case SDLK_X: {
        std::string err;
        if (pc::exportSaveFile(pet, exportPath, err)) std::printf("exported %s\n", exportPath.c_str());
        else std::printf("export failed: %s\n", err.c_str());
        break;
      }
      case SDLK_I:
        if (shift) importFrom(exportPath);
        break;
      default: break;
    }
    std::fflush(stdout);
  };
  // --keys: SDL3 keycodes for letters are the lowercase characters, digits are themselves
  for (char c : keys) handleKey((SDL_Keycode)(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c), c >= 'A' && c <= 'Z');
  uint32_t tapAt = ticks() + 200;   // replay --taps after the first frames
  size_t tapPos = 0;
  uint32_t shotAt = ticks() + shotMs;
  bool mouseDown = false;
  int mx = 0, my = 0;
  uint32_t wait = 0;

  while (running) {
    SDL_Event ev;
    // Sleep until input arrives or the game needs the next step (near 0% CPU when idle).
    bool got = SDL_WaitEventTimeout(&ev, (Sint32)(wait > 20 && (!shot.empty() || !taps.empty()) ? 20 : wait));
    while (got) {
      SDL_ConvertEventToRenderCoordinates(ren, &ev);   // window points -> 466x466 screen
      if (ev.type == SDL_EVENT_QUIT) running = false;
      if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_LEFT) mouseDown = true;
      if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP && ev.button.button == SDL_BUTTON_LEFT) mouseDown = false;
      if (ev.type == SDL_EVENT_MOUSE_MOTION) { mx = (int)ev.motion.x; my = (int)ev.motion.y; }
      if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        mx = (int)ev.button.x;
        my = (int)ev.button.y;
      }
      if (ev.type == SDL_EVENT_KEY_DOWN) handleKey(ev.key.key, ev.key.mod & SDL_KMOD_SHIFT);
      got = SDL_PollEvent(&ev);
    }
    game.pointer(mouseDown, mx, my);

    uint32_t now = ticks();
    if (tapPos < taps.size() && (int32_t)(now - tapAt) >= 0) {   // scripted tap: press 60 ms, release
      char c = taps[tapPos];
      size_t semi = taps.find(';', tapPos);
      if (c == 'U' || c == 'D' || c == 'L' || c == 'R') {   // scripted swipe: U/D/L/R
        game.swipe(c == 'U' ? tama::Gesture::SwipeUp : c == 'D' ? tama::Gesture::SwipeDown
                   : c == 'L' ? tama::Gesture::SwipeLeft : tama::Gesture::SwipeRight, now);
      } else if (c == 'H') {   // H<x>,<y>: hold 3 s (long press)
        int tx = 0, ty = 0;
        std::sscanf(taps.c_str() + tapPos + 1, "%d,%d", &tx, &ty);
        tama::Gesture g;
        g.kind = tama::Gesture::LongPress;
        g.x = (int16_t)tx;
        g.y = (int16_t)ty;
        game.gesture(g, now);
      } else {
        int tx = 0, ty = 0;
        std::sscanf(taps.c_str() + tapPos, "%d,%d", &tx, &ty);
        tama::GestureRecognizer &gr = game.gestures();
        game.gesture(gr.feed(true, (int16_t)tx, (int16_t)ty, now), now);
        game.gesture(gr.feed(false, (int16_t)tx, (int16_t)ty, now + 60), now + 60);
      }
      tapPos = semi == std::string::npos ? taps.size() : semi + 1;
      tapAt = now + 400;
    }
    if (!shot.empty() && (int32_t)(now - shotAt) >= 0) {
      game.renderNow(now);
      SDL_Surface *surf = SDL_CreateSurfaceFrom(kScreenW, kScreenH, SDL_PIXELFORMAT_RGB565,
                                                (void *)canvas.pixels(), kScreenW * 2);
      SDL_SaveBMP(surf, shot.c_str());
      SDL_DestroySurface(surf);
      std::printf("saved %s\n", shot.c_str());
      running = false;
    }
    wait = game.step(now, true);
  }
  game.shutdown();
  pc::audioClose();
  SDL_DestroyTexture(mask);
  SDL_DestroyTexture(tex);
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
