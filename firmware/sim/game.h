#pragma once
// The simulator's game loop: pet + saves + gesture sampling + render pacing, i.e. what src/main.cpp's loop() does on the board.
// The front end owns the window and feeds it pointer state and the time.
#include <cstdint>
#include <functional>
#include <string>

#include "gesture.h"
#include "pet.h"
#include "platform_pc.h"

namespace pc {

class Game {
 public:
  explicit Game(const std::string &saveRoot);   // saves go to <saveRoot>/<theme namespace>/

  tama::Pet &pet() { return pet_; }
  const std::string &saveDir() const { return kv_.dir(); }
  tama::GestureRecognizer &gestures() { return gestures_; }

  // Wall clock (local time as unix seconds). The simulator adds its "next day" offset.
  std::function<uint32_t()> epoch;

  void begin(uint32_t nowMs);
  void pointer(bool down, int x, int y);   // mouse on the glass (0..465), before any rotation
  void swipe(tama::Gesture::Kind kind, uint32_t nowMs);
  void gesture(const tama::Gesture &g, uint32_t nowMs);

  // One loop iteration. `visible` = false skips rendering (hidden or minimised window, like
  // the board with its screen off). Returns how long the front end may wait for input
  // before calling again.
  uint32_t step(uint32_t nowMs, bool visible);
  void renderNow(uint32_t nowMs);
  void shutdown();   // final save

 private:
  FileKv kv_;
  tama::Rng rng_;
  tama::Pet pet_;
  tama::GestureRecognizer gestures_;
  bool down_ = false;
  int mx_ = 0, my_ = 0;
  uint32_t lastSample_ = 0, lastRender_ = 0, lastSave_ = 0;
};

}  // namespace pc
